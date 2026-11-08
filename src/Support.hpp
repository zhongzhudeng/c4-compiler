#pragma once

#include <cstdint>
#include <exception>
#include <iostream>
#include <string>
#include <string_view>

/// A one-based source position.  The lexer is the only component that
/// produces these; all later phases simply carry them along.
struct SourceLoc {
  uint32_t line = 0;
  uint32_t column = 0;
};

inline bool operator==(SourceLoc a, SourceLoc b) {
  return a.line == b.line && a.column == b.column;
}
inline bool operator!=(SourceLoc a, SourceLoc b) { return !(a == b); }

struct SourceRange {
  SourceLoc begin;
  SourceLoc end;
};

/// Thrown after a diagnostic has already been printed.  The top level turns
/// this into exit code 1; no phase catches it to continue with invalid input.
class CompileError : public std::exception {
public:
  const char *what() const noexcept override { return "compilation failed"; }
};

/// Central diagnostic sink.  The required CLI format is
///   <filename>:<line>:<column>: error: <message>
/// and is produced in exactly one place.
class Diagnostics {
public:
  explicit Diagnostics(std::string filename) : filename_(std::move(filename)) {}

  const std::string &filename() const { return filename_; }

  [[noreturn]] void error(SourceLoc loc, const std::string &message) const {
    std::cerr << filename_ << ':' << loc.line << ':' << loc.column
              << ": error: " << message << '\n';
    throw CompileError();
  }

private:
  std::string filename_;
};
