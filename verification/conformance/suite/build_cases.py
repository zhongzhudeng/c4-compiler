"""Build and implementation-constraint checks (A-002, A-006)."""

from __future__ import annotations

from .runner import Checks, Context, Reporter


def run_build(ctx: Context, reporter: Reporter):
    checks = Checks()
    checks.check(ctx.c4.is_file(), "c4 binary is missing: %s" % ctx.c4)
    help_proc = ctx.run([ctx.c4, "--help"], timeout=30)
    checks.check(help_proc.returncode == 0,
                 "--help exited %d" % help_proc.returncode)
    for mode in ("--tokenize", "--parse", "--print-ast", "--compile",
                 "--optimize"):
        checks.check(mode in help_proc.stdout + help_proc.stderr,
                     "usage does not mention %s" % mode)
    reporter.record("build", "c4-binary", ["A-002", "A-005"], checks.ok,
                    checks.detail())

    checks = Checks()
    makefile = ctx.root / "Makefile"
    checks.check(makefile.is_file(), "Makefile is missing")
    if makefile.is_file():
        text = makefile.read_text(encoding="utf-8", errors="replace")
        for target in ("all:", "test:", "release:"):
            checks.check(target in text, "Makefile lacks target %s" % target)
        dry = ctx.run(["make", "-n", "all"], cwd=ctx.root, timeout=60)
        checks.check(dry.returncode == 0,
                     "make -n all exited %d: %s"
                     % (dry.returncode, dry.stderr.strip()[:200]))
        checks.check("cmake" in dry.stdout.lower(),
                     "make -n all does not invoke cmake")
    reporter.record("build", "makefile", ["A-002"], checks.ok,
                    checks.detail())

    checks = Checks()
    sources = sorted((ctx.root / "src").glob("*"))
    cpp = [p for p in sources if p.suffix in (".cpp", ".hpp", ".h")]
    # .def files are preprocessor fragments included by C++ sources.
    other = [p for p in sources
             if p.is_file() and p.suffix not in (".cpp", ".hpp", ".h",
                                                  ".def")]
    checks.check(bool(cpp), "no C++ sources under src/")
    checks.check(not other, "unexpected non-C++ files under src/: %s"
                 % [p.name for p in other])
    cmake = (ctx.root / "CMakeLists.txt").read_text(encoding="utf-8")
    checks.check("CXX" in cmake or "cxx_std" in cmake,
                 "CMakeLists.txt does not configure C++")
    reporter.record("build", "cpp-sources", ["A-006"], checks.ok,
                    checks.detail())
