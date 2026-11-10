#pragma once

#include "AST.hpp"
#include "CodeGen.hpp"
#include "Lexer.hpp"
#include "Optimizer.hpp"
#include "Parser.hpp"
#include "Printer.hpp"
#include "SCCP.hpp"
#include "Sema.hpp"
#include "SemanticModel.hpp"
#include "Support.hpp"
#include "SymbolInternalizer.hpp"
#include "Type.hpp"

#include "llvm/IR/DataLayout.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/raw_ostream.h"

#include <exception>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

/// Captures diagnostics written to std::cerr for the lifetime of the object.
class ScopedCerrCapture {
public:
    ScopedCerrCapture() : old_(std::cerr.rdbuf(buffer_.rdbuf())) {}
    ~ScopedCerrCapture() {
        std::cerr.rdbuf(old_);
    }

    std::string text() const {
        return buffer_.str();
    }

private:
    std::ostringstream buffer_;
    std::streambuf* old_;
};

/// Captures output written to std::cout for the lifetime of the object.
class ScopedCoutCapture {
public:
    ScopedCoutCapture() : old_(std::cout.rdbuf(buffer_.rdbuf())) {}
    ~ScopedCoutCapture() {
        std::cout.rdbuf(old_);
    }

    std::string text() const {
        return buffer_.str();
    }

private:
    std::ostringstream buffer_;
    std::streambuf* old_;
};

/// Convenience front end for unit tests.  Every call starts from a fresh AST.
class TestCompiler {
public:
    bool parse(const std::string& source, std::string* error = nullptr) {
        ast_ = AST();
        ScopedCerrCapture capture;
        Diagnostics diagnostics("test.c");
        try {
            Lexer lexer("test.c", source, symbols_);
            std::vector<Token> tokens = lexer.tokenize();
            Parser parser(ast_, std::move(tokens), diagnostics);
            parser.parse_translation_unit();
        } catch (const CompileError&) {
            if (error)
                *error = capture.text();
            return false;
        }
        return true;
    }

    bool analyze(const std::string& source, std::string* error = nullptr) {
        ScopedCerrCapture capture;
        if (!parse(source, error))
            return false;
        Diagnostics diagnostics("test.c");
        types_ = std::make_unique<TypeContext>();
        context_ = std::make_unique<llvm::LLVMContext>();
        data_layout_ = std::make_unique<llvm::DataLayout>("e-m:o-i64:64-i128:128-n32:64-S128");
        layout_ = std::make_unique<TypeLayout>(*context_, *data_layout_);
        model_ = std::make_unique<SemanticModel>(ast_);
        try {
            Sema sema(ast_, *model_, *types_, *layout_, diagnostics);
            sema.analyze();
        } catch (const CompileError&) {
            if (error)
                *error = capture.text();
            return false;
        }
        return true;
    }

    /// Runs CodeGen against the AST/model produced by analyze().  Tests can
    /// mutate the AST after semantic analysis to exercise lowering invariants.
    bool generate(std::string* ir = nullptr, std::string* error = nullptr) {
        if (!model_ || !types_ || !layout_ || !context_ || !data_layout_) {
            if (error)
                *error = "no analyzed program";
            return false;
        }
        try {
            llvm::Module module("test", *context_);
            module.setDataLayout(*data_layout_);
            CodeGen codegen(ast_, *model_, *types_, *layout_, *context_, module);
            codegen.emit();

            std::string text;
            llvm::raw_string_ostream stream(text);
            module.print(stream, nullptr);
            stream.flush();
            if (ir)
                *ir = text;
        } catch (const std::exception& exception) {
            if (error)
                *error = exception.what();
            return false;
        }
        return true;
    }

    bool print(const std::string& source, std::string* output, std::string* error = nullptr) {
        ScopedCerrCapture capture;
        if (!parse(source, error))
            return false;
        std::ostringstream stream;
        Printer printer(ast_, stream);
        printer.print();
        if (output)
            *output = stream.str();
        return true;
    }

    /// Runs the full pipeline and returns the textual LLVM IR.  With optimize
    /// set, mem2reg and our SCCP are applied first.
    bool compile(const std::string& source, bool optimize, std::string* ir,
                 std::string* error = nullptr) {
        ScopedCerrCapture capture;
        ast_ = AST();
        Diagnostics diagnostics("test.c");
        try {
            Lexer lexer("test.c", source, symbols_);
            std::vector<Token> tokens = lexer.tokenize();
            Parser parser(ast_, std::move(tokens), diagnostics);
            parser.parse_translation_unit();

            TypeContext types;
            llvm::LLVMContext context;
            llvm::DataLayout data_layout("e-m:o-i64:64-i128:128-n32:64-S128");
            TypeLayout layout(context, data_layout);
            SemanticModel model(ast_);
            Sema sema(ast_, model, types, layout, diagnostics);
            sema.analyze();

            llvm::Module module("test", context);
            module.setDataLayout(data_layout);
            CodeGen codegen(ast_, model, types, layout, context, module);
            codegen.emit();

            if (optimize) {
                run_mem2_reg(module);
                SCCP sccp(module);
                sccp.run();

                std::string verification_error;
                llvm::raw_string_ostream verification_stream(verification_error);
                if (llvm::verifyModule(module, &verification_stream)) {
                    verification_stream.flush();
                    throw std::runtime_error("optimized IR failed verification: " +
                                             verification_error);
                }
            }

            std::string text;
            llvm::raw_string_ostream stream(text);
            module.print(stream, nullptr);
            stream.flush();
            if (ir)
                *ir = text;
        } catch (const CompileError&) {
            if (error)
                *error = capture.text();
            return false;
        } catch (const std::exception& exception) {
            if (error)
                *error = exception.what();
            return false;
        }
        return true;
    }

    AST& ast() {
        return ast_;
    }
    const AST& ast() const {
        return ast_;
    }
    const SemanticModel& model() const {
        return *model_;
    }

private:
    SymbolInternalizer symbols_;
    AST ast_;
    // Owned so the semantic model, its types and its layout stay inspectable
    // after analyze().  Destruction runs in reverse declaration order.
    std::unique_ptr<TypeContext> types_;
    std::unique_ptr<llvm::LLVMContext> context_;
    std::unique_ptr<llvm::DataLayout> data_layout_;
    std::unique_ptr<TypeLayout> layout_;
    std::unique_ptr<SemanticModel> model_;
};
