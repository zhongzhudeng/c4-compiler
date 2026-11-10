#include "Compiler.hpp"

#include "TestSupport.hpp"

#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/raw_ostream.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <utility>

namespace {

const Decl& first_decl(const AST& ast) {
    return ast.decl(ast.translation_unit().decls.at(0));
}

Symbol first_declarator_name(const AST& ast) {
    const Decl& decl = first_decl(ast);
    if (const auto* var = std::get_if<VarDecl>(&decl.node))
        return declarator_name(ast, var->declarator);
    if (const auto* function = std::get_if<FunctionDecl>(&decl.node))
        return declarator_name(ast, function->declarator);
    return Symbol();
}

std::string read_text(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

void write_text(const std::filesystem::path& path, const std::string& text) {
    std::ofstream stream(path, std::ios::binary);
    stream << text;
}

/// A fresh directory per test; removal is best-effort so a failed assertion
/// does not hide the original failure.
class TempDirectory {
public:
    TempDirectory() {
        static int counter = 0;
        auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path = std::filesystem::temp_directory_path() /
               ("c4-pipeline-" + std::to_string(stamp) + "-" + std::to_string(counter++));
        std::filesystem::create_directories(path);
    }

    ~TempDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }

    TempDirectory(const TempDirectory&) = delete;
    TempDirectory& operator=(const TempDirectory&) = delete;

    std::filesystem::path path;
};

class ScopedCurrentPath {
public:
    explicit ScopedCurrentPath(const std::filesystem::path& path)
        : previous_(std::filesystem::current_path()) {
        std::filesystem::current_path(path);
    }

    ~ScopedCurrentPath() {
        std::error_code error;
        std::filesystem::current_path(previous_, error);
    }

    ScopedCurrentPath(const ScopedCurrentPath&) = delete;
    ScopedCurrentPath& operator=(const ScopedCurrentPath&) = delete;

private:
    std::filesystem::path previous_;
};

/// A frontend environment equivalent to run()'s frontend-only setup: no target
/// registration, no TargetMachine.
struct FrontendFixture {
    TypeContext types;
    llvm::LLVMContext context;
    llvm::DataLayout data_layout{""};
    TypeLayout layout{context, data_layout};
};

} // namespace

TEST_CASE("tokenization owns the symbols its tokens reference", "[pipeline]") {
    TokenizedSource source = tokenize_string("pipeline.c", "int main(void) { return 7; }");

    CHECK(source.filename() == "pipeline.c");
    REQUIRE_FALSE(source.tokens().empty());
    CHECK(source.tokens().back().kind == TkEoi);

    const Token& first = source.tokens().front();
    REQUIRE(first.text != nullptr);
    CHECK(*first.text == "int");
    REQUIRE(first.file_name != nullptr);
    CHECK(*first.file_name == "pipeline.c");
}

TEST_CASE("parsing keeps interned names alive after tokens are gone", "[pipeline]") {
    ParsedProgram parsed = [] {
        TokenizedSource source =
            tokenize_string("pipeline.c", "int counter; int main(void) { return counter; }");
        return parse(std::move(source));
    }();

    REQUIRE(parsed.filename() == "pipeline.c");
    REQUIRE(parsed.ast().translation_unit().decls.size() == 2);

    Symbol name = first_declarator_name(parsed.ast());
    REQUIRE(name != nullptr);
    CHECK(*name == "counter");
}

TEST_CASE("analysis records a semantic model for a completed AST", "[pipeline]") {
    ParsedProgram parsed = parse(tokenize_string("pipeline.c", "int main(void) { return 0; }"));

    FrontendFixture frontend;
    AnalyzedProgram analyzed = analyze(std::move(parsed), frontend.types, frontend.layout);

    CHECK(&analyzed.types() == &frontend.types);
    CHECK(&analyzed.layout() == &frontend.layout);
    REQUIRE(analyzed.model().entity_count() == 1);

    Symbol name = first_declarator_name(analyzed.ast());
    REQUIRE(name != nullptr);
    CHECK(*name == "main");

    EntityId entity = analyzed.model().binding(analyzed.model().file_scope(), name);
    REQUIRE(entity.valid());
    const Entity& main_entity = analyzed.model().entity(entity);
    CHECK(main_entity.kind == EntityKind::Function);
    REQUIRE(main_entity.type != nullptr);
    CHECK(Type::is_function(main_entity.type));
    CHECK(main_entity.definition == DefinitionState::Defined);
}

TEST_CASE("code generation uses the analyzed program and target layout", "[pipeline]") {
    TargetEnvironment target;
    TypeContext types;
    AnalyzedProgram analyzed =
        analyze(parse(tokenize_string("pipeline.c", "int main(void) { int x; x = 42; return x; }")),
                types, target.layout());

    std::unique_ptr<llvm::Module> module = generate_module(analyzed, target, "pipeline.ll");
    REQUIRE(module != nullptr);
    CHECK(module->getModuleIdentifier() == "pipeline.ll");
    CHECK(module->getTargetTriple().str() == target.triple());
    CHECK(module->getDataLayoutStr() == target.data_layout().getStringRepresentation());

    llvm::Function* main = module->getFunction("main");
    REQUIRE(main != nullptr);
    CHECK_FALSE(main->empty());

    std::string error;
    llvm::raw_string_ostream stream(error);
    CHECK_FALSE(llvm::verifyModule(*module, &stream));
}

TEST_CASE("frontend phases run without a target environment", "[pipeline]") {
    FrontendFixture frontend;
    AnalyzedProgram analyzed =
        analyze(parse(tokenize_string("pipeline.c", "int main(void) { return sizeof(int); }")),
                frontend.types, frontend.layout);

    std::ostringstream printed;
    Printer printer(analyzed.ast(), printed);
    printer.print();
    CHECK(printed.str().find("int (main(void))") != std::string::npos);
    CHECK(printed.str().find("(sizeof(int))") != std::string::npos);
}

TEST_CASE("phase failures produce no stage result", "[pipeline]") {
    ScopedCerrCapture errors;

    REQUIRE_THROWS_AS(parse(tokenize_string("bad.c", "int f( {")), CompileError);

    FrontendFixture frontend;
    REQUIRE_THROWS_AS(analyze(parse(tokenize_string("bad.c", "int main(void) { return missing; }")),
                              frontend.types, frontend.layout),
                      CompileError);

    CHECK(errors.text().find("bad.c:1:") != std::string::npos);
    CHECK(errors.text().find("error:") != std::string::npos);
}

TEST_CASE("independent invocations do not share symbols or models", "[pipeline]") {
    ParsedProgram first = parse(tokenize_string("first.c", "int alpha;"));
    ParsedProgram second = parse(tokenize_string("second.c", "int beta;"));

    Symbol first_name = first_declarator_name(first.ast());
    Symbol second_name = first_declarator_name(second.ast());
    REQUIRE(first_name != nullptr);
    REQUIRE(second_name != nullptr);
    CHECK(*first_name == "alpha");
    CHECK(*second_name == "beta");

    FrontendFixture first_frontend;
    FrontendFixture second_frontend;
    AnalyzedProgram first_analyzed =
        analyze(std::move(first), first_frontend.types, first_frontend.layout);
    AnalyzedProgram second_analyzed =
        analyze(std::move(second), second_frontend.types, second_frontend.layout);

    REQUIRE(first_analyzed.model().entity_count() == 1);
    REQUIRE(second_analyzed.model().entity_count() == 1);
    CHECK(
        first_analyzed.model()
            .entity(first_analyzed.model().binding(first_analyzed.model().file_scope(), first_name))
            .name == first_name);
    CHECK(second_analyzed.model()
              .entity(second_analyzed.model().binding(second_analyzed.model().file_scope(),
                                                      second_name))
              .name == second_name);

    CHECK(first_analyzed.types().int_type() == first_frontend.types.int_type());
    CHECK(second_analyzed.types().int_type() == second_frontend.types.int_type());
    CHECK(first_analyzed.types().int_type() != second_analyzed.types().int_type());
}

TEST_CASE("run executes frontend-only modes without writing IR", "[pipeline]") {
    TempDirectory directory;
    std::filesystem::path source = directory.path / "prog.c";
    write_text(source, "int main(void) { return 5; }\n");
    ScopedCurrentPath current(directory.path);

    {
        ScopedCoutCapture output;
        CHECK(run(source.string(), CompileMode::Tokenize) == 0);
        CHECK(output.text().find("keyword int") != std::string::npos);
        CHECK(output.text().find("identifier main") != std::string::npos);
    }
    CHECK_FALSE(std::filesystem::exists(directory.path / "prog.ll"));

    CHECK(run(source.string(), CompileMode::Parse) == 0);
    CHECK_FALSE(std::filesystem::exists(directory.path / "prog.ll"));

    {
        ScopedCoutCapture output;
        CHECK(run(source.string(), CompileMode::PrintAst) == 0);
        CHECK(output.text().find("int (main(void))") != std::string::npos);
    }
    CHECK_FALSE(std::filesystem::exists(directory.path / "prog.ll"));
}

TEST_CASE("run writes IR for compile and optimize modes", "[pipeline]") {
    TempDirectory directory;
    std::filesystem::create_directories(directory.path / "nested");
    write_text(directory.path / "nested" / "prog.c",
               "int main(void) { int x; x = 42; return x; }\n");
    ScopedCurrentPath current(directory.path);

    CHECK(run("nested/prog.c", CompileMode::Compile) == 0);
    REQUIRE(std::filesystem::exists(directory.path / "prog.ll"));
    CHECK_FALSE(std::filesystem::exists(directory.path / "nested" / "prog.ll"));
    std::string compiled = read_text(directory.path / "prog.ll");
    CHECK(compiled.find("define i32 @main") != std::string::npos);

    CHECK(run("nested/prog.c", CompileMode::Optimize) == 0);
    std::string optimized = read_text(directory.path / "prog.ll");
    CHECK(optimized.find("ret i32 42") != std::string::npos);
    CHECK(optimized.find("alloca") == std::string::npos);
}

TEST_CASE("run reports source failures and leaves no output", "[pipeline]") {
    TempDirectory directory;
    write_text(directory.path / "bad.c", "int main(void) { return missing; }\n");
    ScopedCurrentPath current(directory.path);

    ScopedCerrCapture errors;
    CHECK_THROWS_AS(run("bad.c", CompileMode::Compile), CompileError);
    CHECK(errors.text().find("bad.c:1:") != std::string::npos);
    CHECK(errors.text().find("error:") != std::string::npos);
    CHECK_FALSE(std::filesystem::exists(directory.path / "bad.ll"));
}

TEST_CASE("default invocation matches --compile", "[pipeline][cli]") {
    TempDirectory default_directory;
    TempDirectory explicit_directory;
    const std::string source = "int main(void) { return 9; }\n";
    write_text(default_directory.path / "prog.c", source);
    write_text(explicit_directory.path / "prog.c", source);

    auto invoke = [](const std::filesystem::path& directory, const std::string& arguments) {
        std::string command = "cd '" + directory.string() + "' && '" + C4_EXECUTABLE + "' " +
                              arguments + " > /dev/null 2>&1";
        return std::system(command.c_str());
    };

    CHECK(invoke(default_directory.path, "prog.c") == 0);
    CHECK(invoke(explicit_directory.path, "--compile prog.c") == 0);

    REQUIRE(std::filesystem::exists(default_directory.path / "prog.ll"));
    REQUIRE(std::filesystem::exists(explicit_directory.path / "prog.ll"));
    CHECK(read_text(default_directory.path / "prog.ll") ==
          read_text(explicit_directory.path / "prog.ll"));
}
