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

const Decl &firstDecl(const AST &ast) {
  return ast.decl(ast.translationUnit().decls.at(0));
}

Symbol firstDeclaratorName(const AST &ast) {
  const Decl &decl = firstDecl(ast);
  if (const auto *var = std::get_if<VarDecl>(&decl.node))
    return declaratorName(ast, var->declarator);
  if (const auto *function = std::get_if<FunctionDecl>(&decl.node))
    return declaratorName(ast, function->declarator);
  return nullptr;
}

std::string readText(const std::filesystem::path &path) {
  std::ifstream stream(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(stream),
                     std::istreambuf_iterator<char>());
}

void writeText(const std::filesystem::path &path, const std::string &text) {
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
           ("c4-pipeline-" + std::to_string(stamp) + "-" +
            std::to_string(counter++));
    std::filesystem::create_directories(path);
  }

  ~TempDirectory() {
    std::error_code error;
    std::filesystem::remove_all(path, error);
  }

  TempDirectory(const TempDirectory &) = delete;
  TempDirectory &operator=(const TempDirectory &) = delete;

  std::filesystem::path path;
};

class ScopedCurrentPath {
public:
  explicit ScopedCurrentPath(const std::filesystem::path &path)
      : previous_(std::filesystem::current_path()) {
    std::filesystem::current_path(path);
  }

  ~ScopedCurrentPath() {
    std::error_code error;
    std::filesystem::current_path(previous_, error);
  }

  ScopedCurrentPath(const ScopedCurrentPath &) = delete;
  ScopedCurrentPath &operator=(const ScopedCurrentPath &) = delete;

private:
  std::filesystem::path previous_;
};

/// A frontend environment equivalent to run()'s frontend-only setup: no target
/// registration, no TargetMachine.
struct FrontendFixture {
  TypeContext types;
  llvm::LLVMContext context;
  llvm::DataLayout dataLayout{""};
  TypeLayout layout{context, dataLayout};
};

} // namespace

TEST_CASE("tokenization owns the symbols its tokens reference",
          "[pipeline]") {
  TokenizedSource source =
      tokenizeString("pipeline.c", "int main(void) { return 7; }");

  CHECK(source.filename() == "pipeline.c");
  REQUIRE_FALSE(source.tokens().empty());
  CHECK(source.tokens().back().Kind == TK_EOI);

  const Token &first = source.tokens().front();
  REQUIRE(first.Text != nullptr);
  CHECK(*first.Text == "int");
  REQUIRE(first.FileName != nullptr);
  CHECK(*first.FileName == "pipeline.c");
}

TEST_CASE("parsing keeps interned names alive after tokens are gone",
          "[pipeline]") {
  ParsedProgram parsed = [] {
    TokenizedSource source = tokenizeString(
        "pipeline.c", "int counter; int main(void) { return counter; }");
    return parse(std::move(source));
  }();

  REQUIRE(parsed.filename() == "pipeline.c");
  REQUIRE(parsed.ast().translationUnit().decls.size() == 2);

  Symbol name = firstDeclaratorName(parsed.ast());
  REQUIRE(name != nullptr);
  CHECK(*name == "counter");
}

TEST_CASE("analysis records a semantic model for a completed AST",
          "[pipeline]") {
  ParsedProgram parsed =
      parse(tokenizeString("pipeline.c", "int main(void) { return 0; }"));

  FrontendFixture frontend;
  AnalyzedProgram analyzed =
      analyze(std::move(parsed), frontend.types, frontend.layout);

  CHECK(&analyzed.types() == &frontend.types);
  CHECK(&analyzed.layout() == &frontend.layout);
  REQUIRE(analyzed.model().entityCount() == 1);

  Symbol name = firstDeclaratorName(analyzed.ast());
  REQUIRE(name != nullptr);
  CHECK(*name == "main");

  EntityId entity =
      analyzed.model().binding(analyzed.model().fileScope(), name);
  REQUIRE(entity.valid());
  const Entity &mainEntity = analyzed.model().entity(entity);
  CHECK(mainEntity.kind == EntityKind::Function);
  REQUIRE(mainEntity.type != nullptr);
  CHECK(Type::isFunction(mainEntity.type));
  CHECK(mainEntity.definition == DefinitionState::Defined);
}

TEST_CASE("code generation uses the analyzed program and target layout",
          "[pipeline]") {
  TargetEnvironment target;
  TypeContext types;
  AnalyzedProgram analyzed = analyze(
      parse(tokenizeString(
          "pipeline.c", "int main(void) { int x; x = 42; return x; }")),
      types, target.layout());

  std::unique_ptr<llvm::Module> module =
      generateModule(analyzed, target, "pipeline.ll");
  REQUIRE(module != nullptr);
  CHECK(module->getModuleIdentifier() == "pipeline.ll");
  CHECK(module->getTargetTriple().str() == target.triple());
  CHECK(module->getDataLayoutStr() ==
        target.dataLayout().getStringRepresentation());

  llvm::Function *main = module->getFunction("main");
  REQUIRE(main != nullptr);
  CHECK_FALSE(main->empty());

  std::string error;
  llvm::raw_string_ostream stream(error);
  CHECK_FALSE(llvm::verifyModule(*module, &stream));
}

TEST_CASE("frontend phases run without a target environment", "[pipeline]") {
  FrontendFixture frontend;
  AnalyzedProgram analyzed =
      analyze(parse(tokenizeString("pipeline.c",
                                   "int main(void) { return sizeof(int); }")),
              frontend.types, frontend.layout);

  std::ostringstream printed;
  Printer printer(analyzed.ast(), analyzed.model(), printed);
  printer.print();
  CHECK(printed.str().find("int (main(void))") != std::string::npos);
  CHECK(printed.str().find("(sizeof(int))") != std::string::npos);
}

TEST_CASE("phase failures produce no stage result", "[pipeline]") {
  ScopedCerrCapture errors;

  REQUIRE_THROWS_AS(parse(tokenizeString("bad.c", "int f( {")),
                    CompileError);

  FrontendFixture frontend;
  REQUIRE_THROWS_AS(
      analyze(parse(tokenizeString(
                  "bad.c", "int main(void) { return missing; }")),
              frontend.types, frontend.layout),
      CompileError);

  CHECK(errors.text().find("bad.c:1:") != std::string::npos);
  CHECK(errors.text().find("error:") != std::string::npos);
}

TEST_CASE("independent invocations do not share symbols or models",
          "[pipeline]") {
  ParsedProgram first = parse(tokenizeString("first.c", "int alpha;"));
  ParsedProgram second = parse(tokenizeString("second.c", "int beta;"));

  Symbol firstName = firstDeclaratorName(first.ast());
  Symbol secondName = firstDeclaratorName(second.ast());
  REQUIRE(firstName != nullptr);
  REQUIRE(secondName != nullptr);
  CHECK(*firstName == "alpha");
  CHECK(*secondName == "beta");

  FrontendFixture firstFrontend;
  FrontendFixture secondFrontend;
  AnalyzedProgram firstAnalyzed =
      analyze(std::move(first), firstFrontend.types, firstFrontend.layout);
  AnalyzedProgram secondAnalyzed =
      analyze(std::move(second), secondFrontend.types, secondFrontend.layout);

  REQUIRE(firstAnalyzed.model().entityCount() == 1);
  REQUIRE(secondAnalyzed.model().entityCount() == 1);
  CHECK(firstAnalyzed.model()
            .entity(firstAnalyzed.model().binding(
                firstAnalyzed.model().fileScope(), firstName))
            .name == firstName);
  CHECK(secondAnalyzed.model()
            .entity(secondAnalyzed.model().binding(
                secondAnalyzed.model().fileScope(), secondName))
            .name == secondName);

  CHECK(firstAnalyzed.types().intType() == firstFrontend.types.intType());
  CHECK(secondAnalyzed.types().intType() == secondFrontend.types.intType());
  CHECK(firstAnalyzed.types().intType() != secondAnalyzed.types().intType());
}

TEST_CASE("run executes frontend-only modes without writing IR",
          "[pipeline]") {
  TempDirectory directory;
  std::filesystem::path source = directory.path / "prog.c";
  writeText(source, "int main(void) { return 5; }\n");
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
  writeText(directory.path / "nested" / "prog.c",
            "int main(void) { int x; x = 42; return x; }\n");
  ScopedCurrentPath current(directory.path);

  CHECK(run("nested/prog.c", CompileMode::Compile) == 0);
  REQUIRE(std::filesystem::exists(directory.path / "prog.ll"));
  CHECK_FALSE(
      std::filesystem::exists(directory.path / "nested" / "prog.ll"));
  std::string compiled = readText(directory.path / "prog.ll");
  CHECK(compiled.find("define i32 @main") != std::string::npos);

  CHECK(run("nested/prog.c", CompileMode::Optimize) == 0);
  std::string optimized = readText(directory.path / "prog.ll");
  CHECK(optimized.find("ret i32 42") != std::string::npos);
  CHECK(optimized.find("alloca") == std::string::npos);
}

TEST_CASE("run reports source failures and leaves no output", "[pipeline]") {
  TempDirectory directory;
  writeText(directory.path / "bad.c",
            "int main(void) { return missing; }\n");
  ScopedCurrentPath current(directory.path);

  ScopedCerrCapture errors;
  CHECK_THROWS_AS(run("bad.c", CompileMode::Compile), CompileError);
  CHECK(errors.text().find("bad.c:1:") != std::string::npos);
  CHECK(errors.text().find("error:") != std::string::npos);
  CHECK_FALSE(std::filesystem::exists(directory.path / "bad.ll"));
}

TEST_CASE("default invocation matches --compile", "[pipeline][cli]") {
  TempDirectory defaultDirectory;
  TempDirectory explicitDirectory;
  const std::string source = "int main(void) { return 9; }\n";
  writeText(defaultDirectory.path / "prog.c", source);
  writeText(explicitDirectory.path / "prog.c", source);

  auto invoke = [](const std::filesystem::path &directory,
                   const std::string &arguments) {
    std::string command = "cd '" + directory.string() + "' && '" +
                          C4_EXECUTABLE + "' " + arguments +
                          " > /dev/null 2>&1";
    return std::system(command.c_str());
  };

  CHECK(invoke(defaultDirectory.path, "prog.c") == 0);
  CHECK(invoke(explicitDirectory.path, "--compile prog.c") == 0);

  REQUIRE(std::filesystem::exists(defaultDirectory.path / "prog.ll"));
  REQUIRE(std::filesystem::exists(explicitDirectory.path / "prog.ll"));
  CHECK(readText(defaultDirectory.path / "prog.ll") ==
        readText(explicitDirectory.path / "prog.ll"));
}
