"""Shared infrastructure for the C4 conformance harness.

The harness drives the real compiler (c4) and compares its observable
behaviour with the independent reference frontend in ../c4ref.  It is not a
proof of correctness: every result is evidence over the corpus named in the
evidence JSON and the coverage report.
"""

from __future__ import annotations

import os
import re
import shutil
import subprocess
import tempfile
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, Iterable, List, Optional, Sequence, Tuple

SUITE_NAMES = ("build", "cli", "lexer", "parser", "sema", "printer",
               "codegen", "sccp")

DIAGNOSTIC_RE = re.compile(r"^(?P<file>.*?):(?P<line>\d+):(?P<column>\d+): "
                           r"error: (?P<message>.*)$")

TOKEN_RE = re.compile(r"^(?P<line>\d+):(?P<column>\d+): "
                      r"(?P<kind>[a-z][a-z-]*)\s(?P<text>.*)$")


@dataclass
class Proc:
    args: Tuple[str, ...]
    returncode: int
    stdout: str
    stderr: str
    seconds: float = 0.0

    @property
    def ok(self) -> bool:
        return self.returncode == 0

    def diagnostics(self) -> List[re.Match]:
        return [m for m in (DIAGNOSTIC_RE.match(line)
                            for line in self.stderr.splitlines()) if m]


@dataclass
class CaseResult:
    suite: str
    case: str
    requirements: Tuple[str, ...]
    passed: bool
    detail: str = ""
    skipped: bool = False


class Reporter:
    def __init__(self, verbose: bool = False):
        self.verbose = verbose
        self.results: List[CaseResult] = []

    def record(self, suite: str, case: str, requirements: Iterable[str],
               passed: bool, detail: str = "", skipped: bool = False):
        result = CaseResult(suite, case, tuple(requirements), passed,
                            detail, skipped)
        self.results.append(result)
        if skipped:
            if self.verbose:
                print("skip %s/%s: %s" % (suite, case, detail))
        elif passed:
            if self.verbose:
                print("ok   %s/%s" % (suite, case))
        else:
            print("FAIL %s/%s: %s" % (suite, case, detail))
        return passed

    def check(self, suite: str, case: str, requirements: Iterable[str],
              condition: bool, detail: str = ""):
        return self.record(suite, case, requirements, bool(condition), detail)

    def skip(self, suite: str, case: str, requirements: Iterable[str],
             detail: str):
        return self.record(suite, case, requirements, True, detail,
                           skipped=True)


class Checks:
    """Collects the sub-checks of one reported case."""

    def __init__(self):
        self.failures: List[str] = []
        self.notes: List[str] = []

    def check(self, condition: bool, message: str):
        if not condition:
            self.failures.append(message)
        return condition

    def note(self, message: str):
        self.notes.append(message)

    @property
    def ok(self) -> bool:
        return not self.failures

    def detail(self, limit: int = 8) -> str:
        parts = self.failures[:limit]
        if len(self.failures) > limit:
            parts.append("... and %d more" % (len(self.failures) - limit))
        return "; ".join(parts)


def find_tool(name: str, candidates: Sequence[str] = ()) -> Optional[Path]:
    found = shutil.which(name)
    if found:
        return Path(found)
    for candidate in candidates:
        path = Path(candidate)
        if path.is_file() and os.access(str(path), os.X_OK):
            return path
    return None


class Context:
    def __init__(self, c4: Path, root: Path, workdir: Optional[Path] = None,
                 keep: bool = False):
        self.c4 = Path(c4).resolve()
        self.root = Path(root).resolve()
        self.keep = keep
        self.workdir = Path(workdir or tempfile.mkdtemp(
            prefix="c4-conformance-")).resolve()
        self.workdir.mkdir(parents=True, exist_ok=True)
        self.llvm_bin = Path("/opt/homebrew/opt/llvm/bin")
        self.llvm_as = find_tool(
            "llvm-as", [str(self.llvm_bin / "llvm-as")])
        self.opt = find_tool("opt", [str(self.llvm_bin / "opt")])
        self.clang = find_tool(
            "clang", [str(self.llvm_bin / "clang"), "/usr/bin/clang"])

    def run(self, args: Sequence[object], cwd: Optional[Path] = None,
            timeout: float = 120.0) -> Proc:
        command = [str(a) for a in args]
        start = time.monotonic()
        try:
            completed = subprocess.run(
                command, cwd=str(cwd or self.workdir),
                stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                timeout=timeout)
            stdout = completed.stdout.decode("latin-1")
            stderr = completed.stderr.decode("latin-1")
            code = completed.returncode
        except subprocess.TimeoutExpired as exc:
            stdout = (exc.stdout or b"").decode("latin-1")
            stderr = (exc.stderr or b"").decode("latin-1")
            code = 124
            stderr += "\n[harness] timed out after %ss" % timeout
        return Proc(tuple(command), code, stdout, stderr,
                    time.monotonic() - start)

    def c4run(self, mode: str, filename: str, cwd: Path,
              timeout: float = 120.0) -> Proc:
        return self.run([self.c4, mode, filename], cwd=cwd, timeout=timeout)

    def tempdir(self, prefix: str) -> Path:
        path = Path(tempfile.mkdtemp(prefix=prefix, dir=str(self.workdir)))
        return path

    def cleanup(self, path: Path):
        if self.keep:
            return
        shutil.rmtree(str(path), ignore_errors=True)

    def rel(self, path: Path) -> str:
        try:
            return str(Path(path).resolve().relative_to(self.root))
        except ValueError:
            return str(path)

    def read_source(self, path: Path) -> str:
        return path.read_bytes().decode("latin-1")

    def write_source(self, path: Path, text: str):
        path.write_bytes(text.encode("latin-1"))


DIRECTIVE_RE = re.compile(
    r"^\s*//\s*(exit|stdout|args|modes|error|line)\s*:\s?(.*)$")


def read_directives(path: Path) -> Dict[str, object]:
    directives: Dict[str, object] = {
        "exit": 0, "stdout": "", "args": [],
        "modes": ["compile", "optimize"], "error": None, "line": None,
    }
    stdout_lines: List[str] = []
    for raw in path.read_text(encoding="latin-1").splitlines():
        match = DIRECTIVE_RE.match(raw)
        if not match:
            continue
        key, value = match.group(1), match.group(2)
        if key == "exit":
            directives["exit"] = int(value)
        elif key == "stdout":
            stdout_lines.append(value)
        elif key == "args":
            directives["args"] = value.split()
        elif key == "modes":
            directives["modes"] = [m.strip() for m in value.split(",")
                                   if m.strip()]
        elif key == "error":
            directives["error"] = value
        elif key == "line":
            directives["line"] = int(value)
    directives["stdout"] = "".join(line + "\n" for line in stdout_lines)
    return directives


def parse_token_output(proc: Proc, filename: str
                       ) -> Tuple[List[Tuple[int, int, str, str]], List[str]]:
    """Parse --tokenize stdout, returning tokens and malformed lines."""
    prefix = filename + ":"
    tokens: List[Tuple[int, int, str, str]] = []
    malformed: List[str] = []
    for line in proc.stdout.splitlines():
        if not line.startswith(prefix):
            malformed.append(line)
            continue
        match = TOKEN_RE.match(line[len(prefix):])
        if not match:
            malformed.append(line)
            continue
        tokens.append((int(match.group("line")),
                       int(match.group("column")),
                       match.group("kind"),
                       match.group("text")))
    return tokens, malformed


def reference_tokens(text: str) -> List[Tuple[int, int, str, str]]:
    from c4ref.lexer import lex
    return [(tok.line, tok.column, tok.kind, tok.text)
            for tok in lex(text) if tok.kind != "eof"]


def strip_locations(value):
    """Recursively drop Loc/paren bookkeeping from a reference AST."""
    from dataclasses import is_dataclass, fields
    if isinstance(value, list):
        return [strip_locations(item) for item in value]
    if isinstance(value, tuple):
        return tuple(strip_locations(item) for item in value)
    if is_dataclass(value):
        # The parser keeps an explicit node for source parentheses.  The
        # printer discards them (D-005), so they are not part of the
        # structural comparison.
        if value.__class__.__name__ == "Expr" and value.kind == "paren":
            return strip_locations(value.operand)
        result = {}
        for field_info in fields(value):
            if field_info.name in ("loc", "op_loc", "parenthesized"):
                continue
            result[field_info.name] = strip_locations(
                getattr(value, field_info.name))
        return result
    return value


def ast_equal(left, right) -> bool:
    return strip_locations(left) == strip_locations(right)
