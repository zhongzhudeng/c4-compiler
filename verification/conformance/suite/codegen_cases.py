"""Code generation conformance: IR validity and differential execution.

For every accepted end-to-end program and both compilation modes the harness
checks that c4 emits an LLVM module, that the module passes llvm-as and the
LLVM verifier, that it links, and that the resulting executable agrees with a
native C reference executable on exit status and stdout (F-007, F-008).
Targeted fixtures check the i8/i32 representation and DataLayout use.
"""

from __future__ import annotations

import re
import shutil
from pathlib import Path

from .runner import Checks, Context, Reporter, read_directives


def _verify_ir(ctx: Context, ir: Path, checks: Checks):
    proc = ctx.run([ctx.llvm_as, ir, "-o", "/dev/null"], timeout=60)
    checks.check(proc.returncode == 0,
                 "llvm-as rejected the IR: %s" % proc.stderr.strip()[:200])
    if ctx.opt is not None:
        proc = ctx.run([ctx.opt, "-passes=verify", "-disable-output", ir],
                       timeout=60)
        checks.check(proc.returncode == 0,
                     "LLVM verifier rejected the IR: %s"
                     % proc.stderr.strip()[:200])


def _run_program(ctx: Context, executable: Path, args, cwd: Path,
                 timeout: float = 20.0):
    return ctx.run([executable] + list(args), cwd=cwd, timeout=timeout)


def _check_program(ctx: Context, reporter: Reporter, source_path: Path,
                   mode: str, stem: str):
    directives = read_directives(source_path)
    work = ctx.tempdir("codegen-")
    try:
        source = work / (stem + ".c")
        shutil.copyfile(str(source_path), str(source))
        checks = Checks()
        proc = ctx.c4run("--" + mode, source.name, work)
        checks.check(proc.returncode == 0,
                     "c4 --%s failed: %s"
                     % (mode, proc.stderr.strip()[:200]))
        ir = work / (stem + ".ll")
        checks.check(ir.is_file(), "missing output %s" % ir.name)
        if proc.returncode != 0 or not ir.is_file():
            reporter.record("codegen", "%s[%s]" % (stem, mode),
                            ["F-001", "F-007"], False, checks.detail())
            return
        text = ir.read_text(encoding="latin-1")
        checks.check(text.startswith("; ModuleID"),
                     "IR does not look like human-readable LLVM text")
        checks.check("define " in text or "declare " in text,
                     "IR contains no function definitions")
        _verify_ir(ctx, ir, checks)

        executable = work / "program"
        link = ctx.run([ctx.clang, "-Wno-override-module", str(ir),
                        "-o", str(executable)], cwd=work, timeout=120)
        checks.check(link.returncode == 0,
                     "clang rejected the generated IR: %s"
                     % link.stderr.strip()[:200])
        if link.returncode != 0:
            reporter.record("codegen", "%s[%s]" % (stem, mode),
                            ["F-001", "F-007"], False, checks.detail())
            return

        native = work / "reference"
        native_compile = ctx.run(
            [ctx.clang, "-std=c11", "-Wno-everything", str(source),
             "-o", str(native)], cwd=work, timeout=120)
        checks.check(native_compile.returncode == 0,
                     "native clang rejected the C source: %s"
                     % native_compile.stderr.strip()[:200])
        if native_compile.returncode != 0:
            reporter.record("codegen", "%s[%s]" % (stem, mode),
                            ["F-001", "F-008"], False, checks.detail())
            return

        args = list(directives["args"])
        actual = _run_program(ctx, executable, args, work)
        reference = _run_program(ctx, native, args, work)
        checks.check(actual.returncode == directives["exit"],
                     "exit %d, expected %d"
                     % (actual.returncode, directives["exit"]))
        checks.check(actual.stdout == directives["stdout"],
                     "stdout %r, expected %r"
                     % (actual.stdout, directives["stdout"]))
        checks.check(actual.returncode == reference.returncode,
                     "differential exit: c4 %d, native %d"
                     % (actual.returncode, reference.returncode))
        checks.check(actual.stdout == reference.stdout,
                     "differential stdout: c4 %r, native %r"
                     % (actual.stdout, reference.stdout))
        reporter.record("codegen", "%s[%s]" % (stem, mode),
                        ["F-001", "F-005", "F-007", "F-008"], checks.ok,
                        checks.detail())
    finally:
        ctx.cleanup(work)


def _check_types_and_layout(ctx: Context, reporter: Reporter):
    work = ctx.tempdir("codegen-types-")
    try:
        source = (
            "int f(char c) { return c; }\n"
            "struct S { char c; int i; };\n"
            "int size(void) { return sizeof(struct S); }\n"
            "int main(void) { struct S s; s.c = 1; s.i = 2; "
            "return f(s.c) + sizeof(struct S); }\n")
        ctx.write_source(work / "types.c", source)
        proc = ctx.c4run("--compile", "types.c", work)
        checks = Checks()
        checks.check(proc.returncode == 0,
                     "compile failed: %s" % proc.stderr[:200])
        ir = work / "types.ll"
        checks.check(ir.is_file(), "missing types.ll")
        if ir.is_file():
            text = ir.read_text(encoding="latin-1")
            checks.check(re.search(r"define i32 @f\(i8", text) is not None,
                         "char parameter is not lowered to i8")
            checks.check(re.search(r"ret i32", text) is not None,
                         "int return is not lowered to i32")
            checks.check("target datalayout" in text,
                         "module has no target datalayout")
            # struct S { char; int; } has size 8 under the host DataLayout.
            checks.check(re.search(r"ret i32 8\b", text) is not None,
                         "sizeof(struct S) was not computed as 8")
        reporter.record("codegen", "i8-i32-and-datalayout",
                        ["F-002", "F-003"], checks.ok, checks.detail())
    finally:
        ctx.cleanup(work)


def _check_default_equals_compile(ctx: Context, reporter: Reporter):
    work = ctx.tempdir("codegen-default-")
    try:
        source = ctx.root / "tests" / "e2e" / "programs" / "arith.c"
        shutil.copyfile(str(source), str(work / "arith.c"))
        default = ctx.run([ctx.c4, "arith.c"], cwd=work)
        default_ir = (work / "arith.ll").read_text(encoding="latin-1")
        (work / "arith.ll").unlink()
        explicit = ctx.c4run("--compile", "arith.c", work)
        explicit_ir = (work / "arith.ll").read_text(encoding="latin-1")
        checks = Checks()
        checks.check(default.returncode == 0 and explicit.returncode == 0,
                     "default/--compile failed")
        checks.check(default_ir == explicit_ir,
                     "default mode IR differs from --compile IR")
        reporter.record("codegen", "default-equals-compile", ["F-006"],
                        checks.ok, checks.detail())
    finally:
        ctx.cleanup(work)


def run_codegen(ctx: Context, reporter: Reporter):
    if ctx.llvm_as is None:
        reporter.block("codegen", "llvm-tools", ["F-007"],
                       "llvm-as is not available")
        return
    if ctx.clang is None:
        reporter.block("codegen", "clang", ["F-008"],
                       "clang is not available")
        return

    _check_types_and_layout(ctx, reporter)
    _check_default_equals_compile(ctx, reporter)

    programs_dir = ctx.root / "tests" / "e2e" / "programs"
    for path in sorted(programs_dir.glob("*.c")):
        directives = read_directives(path)
        for mode in directives["modes"]:
            _check_program(ctx, reporter, path, mode, path.stem)
