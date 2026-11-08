"""CLI contract checks (A-003, A-004, A-005, A-007, F-004, F-006)."""

from __future__ import annotations

from .runner import Checks, Context, Reporter, DIAGNOSTIC_RE

VALID = "int main(void) { return 42; }\n"
INVALID = "int main(void) { return missing; }\n"


def run_cli(ctx: Context, reporter: Reporter):
    work = ctx.tempdir("cli-")
    try:
        ctx.write_source(work / "valid.c", VALID)
        ctx.write_source(work / "invalid.c", INVALID)

        proc = ctx.c4run("--parse", "valid.c", work)
        reporter.record(
            "cli", "parse-accepts", ["A-003", "A-005", "C-007"],
            proc.returncode == 0 and not proc.stdout and not proc.stderr,
            "exit %d, stdout %r, stderr %r"
            % (proc.returncode, proc.stdout[:100], proc.stderr[:100]))

        proc = ctx.c4run("--parse", "invalid.c", work)
        matches = [DIAGNOSTIC_RE.match(line)
                   for line in proc.stderr.splitlines()]
        matches = [m for m in matches if m]
        checks = Checks()
        checks.check(proc.returncode == 1,
                     "exit %d, expected 1" % proc.returncode)
        checks.check(bool(matches), "missing diagnostic: %r"
                     % proc.stderr[:200])
        if matches:
            match = matches[0]
            checks.check(match.group("file") == "invalid.c",
                         "filename %r, expected 'invalid.c'"
                         % match.group("file"))
            checks.check(int(match.group("line")) == 1,
                         "line %s, expected 1" % match.group("line"))
            checks.check(int(match.group("column")) > 0,
                         "column is not 1-based")
        reporter.record("cli", "parse-rejects", ["A-004", "A-007"], checks.ok,
                        checks.detail())

        # A-001 acceptance boundary: a C4 program is accepted and compiled,
        # invalid C is rejected, and valid C outside C4 is rejected or
        # supported; the classification is recorded rather than assumed.
        outside = ("int main(void) { for (;;) ; return 0; }\n")
        ctx.write_source(work / "outside.c", outside)
        valid_compile = ctx.c4run("--compile", "valid.c", work)
        outside_proc = ctx.c4run("--parse", "outside.c", work)
        checks = Checks()
        checks.check(valid_compile.returncode == 0,
                     "valid C4 program was not compiled")
        checks.check(proc.returncode == 1 and bool(matches),
                     "invalid C was not rejected with a diagnostic")
        if outside_proc.returncode == 0:
            checks.note("valid C outside C4 was accepted as an extension")
        else:
            checks.check(outside_proc.returncode == 1 and
                         "error:" in outside_proc.stderr,
                         "valid C outside C4 was neither accepted nor "
                         "rejected with a diagnostic")
        reporter.record("cli", "acceptance-boundary", ["A-001"], checks.ok,
                        checks.detail())

        # Every mode on a valid program.
        mode_expectations = [
            ("--tokenize", "tokenize", ["A-005", "L-012"]),
            ("--parse", "parse", ["A-005", "C-007"]),
            ("--print-ast", "print", ["A-005", "D-001"]),
            ("--compile", "compile", ["A-005", "F-001"]),
            ("--optimize", "optimize", ["A-005", "G-001"]),
        ]
        for mode, label, requirements in mode_expectations:
            proc = ctx.c4run(mode, "valid.c", work)
            checks = Checks()
            checks.check(proc.returncode == 0,
                         "%s exited %d: %s"
                         % (mode, proc.returncode, proc.stderr[:160]))
            if mode in ("--compile", "--optimize"):
                checks.check((work / "valid.ll").is_file(),
                             "%s did not write valid.ll" % mode)
            if mode == "--tokenize":
                checks.check("valid.c:1:1: keyword int" in proc.stdout,
                             "unexpected tokenize output")
            if mode == "--print-ast":
                checks.check("int (main(void))" in proc.stdout,
                             "unexpected print output")
            reporter.record("cli", "mode-" + label, requirements, checks.ok,
                            checks.detail())

        # Default mode is --compile, and the output naming rule holds.
        sub = work / "sub"
        sub.mkdir()
        ctx.write_source(sub / "named.c", VALID)
        default = ctx.run([ctx.c4, "sub/named.c"], cwd=work)
        default_ir = None
        if (work / "named.ll").is_file():
            default_ir = (work / "named.ll").read_text(encoding="latin-1")
        checks = Checks()
        checks.check(default.returncode == 0,
                     "default mode exited %d: %s"
                     % (default.returncode, default.stderr[:160]))
        checks.check((work / "named.ll").is_file(),
                     "sub/named.c did not produce named.ll in the cwd")
        checks.check(not (sub / "named.ll").exists(),
                     "output was written next to the input")
        reporter.record("cli", "default-and-naming",
                        ["F-004", "F-006", "A-005"], checks.ok,
                        checks.detail())

        # Default mode and --compile must produce identical IR.
        explicit = ctx.c4run("--compile", "sub/named.c", work)
        explicit_ir = None
        if (work / "named.ll").is_file():
            explicit_ir = (work / "named.ll").read_text(encoding="latin-1")
        checks = Checks()
        checks.check(explicit.returncode == 0,
                     "--compile exited %d" % explicit.returncode)
        checks.check(default_ir is not None and explicit_ir is not None,
                     "default or --compile did not write named.ll")
        if default_ir is not None and explicit_ir is not None:
            checks.check(default_ir == explicit_ir,
                         "default and --compile IR differ")

        # Repeat the comparison in a fresh directory, capturing each output
        # before the next invocation can overwrite it.
        if explicit.returncode == 0:
            work2 = ctx.tempdir("cli-default-")
            try:
                import shutil
                shutil.copytree(str(sub), str(work2 / "sub"))
                default2 = ctx.run([ctx.c4, "sub/named.c"], cwd=work2)
                default2_ir = None
                if (work2 / "named.ll").is_file():
                    default2_ir = (work2 / "named.ll").read_text(
                        encoding="latin-1")
                explicit2 = ctx.run([ctx.c4, "--compile", "sub/named.c"],
                                    cwd=work2)
                explicit2_ir = None
                if (work2 / "named.ll").is_file():
                    explicit2_ir = (work2 / "named.ll").read_text(
                        encoding="latin-1")
                checks.check(default2.returncode == 0,
                             "default mode failed in the second directory")
                checks.check(explicit2.returncode == 0,
                             "--compile failed in the second directory")
                checks.check(default2_ir is not None and
                             explicit2_ir is not None,
                             "default or --compile did not write named.ll "
                             "in the second directory")
                if default2_ir is not None and explicit2_ir is not None:
                    checks.check(default2_ir == explicit2_ir,
                                 "default and --compile IR differ in the "
                                 "second directory")
                    if default_ir is not None:
                        checks.check(default2_ir == default_ir,
                                     "default IR differs between directories")
            finally:
                ctx.cleanup(work2)
        reporter.record("cli", "default-equals-compile",
                        ["F-006"], checks.ok, checks.detail())
    finally:
        ctx.cleanup(work)
