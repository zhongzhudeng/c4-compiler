"""SCCP conformance (Task G).

Each fixture is compiled twice: --compile gives the pre-SCCP module, and
--optimize gives the post-SCCP module.  The harness reconstructs the
post-mem2reg pre-SCCP module with LLVM's mem2reg pass (the only LLVM pass
permitted by G-007) and checks the four required transformations:
constant substitution, branch simplification, dead-instruction elimination,
and unreachable-block elimination.  Optimized executables must preserve the
behaviour of unoptimized ones (G-009).
"""

from __future__ import annotations

import re
import shutil
from pathlib import Path

from .runner import Checks, Context, Reporter

FIXTURES = [
    {
        "name": "constant-substitution",
        "source": """int main(void) {
  int a;
  int b;
  int x;
  int y;
  a = 3;
  b = 4;
  x = a + b;
  y = (x == 7);
  return (y ? x : 0);
}
""",
        "exit": 7,
        "pre_mem2reg": ["add i32 3, 4"],
        "post": ["ret i32 7"],
        "post_absent": ["add i32", "icmp ", "select ", "br i1"],
        "requirements": ["G-003"],
    },
    {
        "name": "branch-simplification",
        "source": """int main(void) {
  int c;
  int x;
  c = 1;
  x = 0;
  if (c) { x = 5; } else { x = 6; }
  return x;
}
""",
        "exit": 5,
        "pre": ["br i1"],
        "post": ["ret i32 5"],
        "post_absent": ["br i1", "ret i32 6"],
        "requirements": ["G-004"],
    },
    {
        "name": "dead-instruction-elimination",
        "source": """int main(void) {
  int a;
  int b;
  int dead;
  int live;
  a = 1;
  b = 2;
  dead = a + b;
  live = 5;
  return live;
}
""",
        "exit": 5,
        "pre_mem2reg": ["add i32 1, 2"],
        "post": ["ret i32 5"],
        "post_absent": ["add i32", "ret i32 3"],
        "requirements": ["G-005"],
    },
    {
        "name": "unreachable-block-elimination",
        "source": """int main(void) {
  int c;
  int x;
  c = 0;
  x = 0;
  if (c) { x = 1; return 1; }
  return x;
}
""",
        "exit": 0,
        "pre": ["br i1", "ret i32 1"],
        "post": ["ret i32 0"],
        "post_absent": ["br i1", "ret i32 1"],
        "requirements": ["G-006"],
    },
    {
        "name": "phi-constant-fold",
        "source": """int main(void) {
  int c;
  int x;
  c = 1;
  if (c) { x = 3; } else { x = 4; }
  return x;
}
""",
        "exit": 3,
        "pre_mem2reg": ["phi "],
        "post": ["ret i32 3"],
        "post_absent": ["br i1", "ret i32 4"],
        "requirements": ["G-003", "G-004"],
    },
    {
        "name": "compare-fold",
        "source": "int main(void) { int a; int b; int r; a = 2; b = 3; "
                  "r = (a < b); return r; }\n",
        "exit": 1,
        "pre_mem2reg": ["icmp "],
        "post": ["ret i32 1"],
        "post_absent": ["icmp "],
        "requirements": ["G-003"],
    },
    {
        "name": "select-fold",
        "source": "int main(void) { int c; int a; c = 1; "
                  "a = c ? 7 : 8; return a; }\n",
        "exit": 7,
        "pre": ["br i1"],
        "post": ["ret i32 7"],
        "post_absent": ["br i1", "ret i32 8"],
        "requirements": ["G-003", "G-004"],
    },
    {
        "name": "cast-fold",
        "source": "int main(void) { char c; int i; c = 65; i = c; return i; }\n",
        "exit": 65,
        "pre_mem2reg": ["sext i8"],
        "post": ["ret i32 65"],
        "post_absent": ["sext ", "zext "],
        "requirements": ["G-003"],
    },
    {
        "name": "arithmetic-chain",
        "source": "int main(void) { int a; int b; int x; a = 2; b = 3; "
                  "x = a * b + 4; return x; }\n",
        "exit": 10,
        "pre_mem2reg": ["mul i32 2, 3"],
        "post": ["ret i32 10"],
        "post_absent": ["mul i32", "add i32"],
        "requirements": ["G-003"],
    },
    {
        "name": "dead-after-return",
        "source": "int main(void) { return 0; int x; x = 1; return x; }\n",
        "exit": 0,
        "pre_mem2reg": ["ret i32 1"],
        "post": ["ret i32 0"],
        "post_absent": ["ret i32 1"],
        "requirements": ["G-006", "G-005"],
    },
    {
        "name": "loop-behaviour",
        "source": """int main(void) {
  int i;
  int sum;
  i = 0;
  sum = 0;
  while (i < 5) { sum = sum + i; i = i + 1; }
  return sum;
}
""",
        "exit": 10,
        "post": ["ret i32"],
        "post_absent": [],
        "requirements": ["G-002", "G-009"],
    },
]


def _verify(ctx: Context, ir: Path, checks: Checks, label: str):
    proc = ctx.run([ctx.llvm_as, ir, "-o", "/dev/null"], timeout=60)
    checks.check(proc.returncode == 0,
                 "%s: llvm-as failed: %s"
                 % (label, proc.stderr.strip()[:200]))
    if ctx.opt is not None:
        proc = ctx.run([ctx.opt, "-passes=verify", "-disable-output", ir],
                       timeout=60)
        checks.check(proc.returncode == 0,
                     "%s: verifier failed: %s"
                     % (label, proc.stderr.strip()[:200]))


def _link_and_run(ctx: Context, ir: Path, work: Path, name: str,
                  checks: Checks):
    executable = work / (name + ".exe")
    link = ctx.run([ctx.clang, "-Wno-override-module", str(ir),
                    "-o", str(executable)], cwd=work, timeout=120)
    checks.check(link.returncode == 0,
                 "%s: clang rejected IR: %s"
                 % (name, link.stderr.strip()[:200]))
    if link.returncode != 0:
        return None
    return ctx.run([executable], cwd=work, timeout=20)


def _source_scan(ctx: Context, checks: Checks):
    optimizer = (ctx.root / "src" / "Optimizer.cpp").read_text(
        encoding="utf-8")
    compiler = (ctx.root / "src" / "Compiler.cpp").read_text(
        encoding="utf-8")
    sccp = (ctx.root / "src" / "SCCP.cpp").read_text(encoding="utf-8")
    passes = re.findall(r"addPass\(([^)]*)\)", optimizer)
    checks.check(bool(passes), "no pass is registered in Optimizer.cpp")
    for registered in passes:
        checks.check("PromotePass" in registered,
                     "unexpected LLVM pass registered: %s" % registered)
    checks.check("runMem2Reg" in compiler,
                 "Compiler.cpp does not run mem2reg")
    checks.check("SCCP sccp" in compiler,
                 "Compiler.cpp does not run the custom SCCP pass")
    checks.check("FunctionSCCP" in sccp,
                 "SCCP.cpp does not contain the custom pass")
    for forbidden in ("PassBuilder", "PassManager", "createPromote"):
        checks.check(forbidden not in sccp,
                     "SCCP.cpp uses an LLVM pass infrastructure: %s"
                     % forbidden)


def run_sccp(ctx: Context, reporter: Reporter):
    if ctx.llvm_as is None or ctx.opt is None:
        reporter.skip("sccp", "llvm-tools", ["G-008"],
                      "llvm-as/opt are not available")
        return
    if ctx.clang is None:
        reporter.skip("sccp", "clang", ["G-009"], "clang is not available")
        return

    checks = Checks()
    _source_scan(ctx, checks)
    reporter.record("sccp", "only-mem2reg", ["G-007"], checks.ok,
                    checks.detail())

    pairs = 0
    for fixture in FIXTURES:
        name = str(fixture["name"])
        source = str(fixture["source"])
        requirements = list(dict.fromkeys(
            list(fixture["requirements"]) + ["G-002", "G-008", "G-009"]))
        work = ctx.tempdir("sccp-" + name + "-")
        try:
            ctx.write_source(work / (name + ".c"), source)
            plain_dir = work / "plain"
            opt_dir = work / "opt"
            plain_dir.mkdir()
            opt_dir.mkdir()
            shutil.copyfile(str(work / (name + ".c")),
                            str(plain_dir / (name + ".c")))
            shutil.copyfile(str(work / (name + ".c")),
                            str(opt_dir / (name + ".c")))

            plain = ctx.c4run("--compile", name + ".c", plain_dir)
            optimized = ctx.c4run("--optimize", name + ".c", opt_dir)
            checks = Checks()
            checks.check(plain.returncode == 0,
                         "c4 --compile failed: %s"
                         % plain.stderr.strip()[:200])
            checks.check(optimized.returncode == 0,
                         "c4 --optimize failed: %s"
                         % optimized.stderr.strip()[:200])
            pre = plain_dir / (name + ".ll")
            post = opt_dir / (name + ".ll")
            checks.check(pre.is_file() and post.is_file(),
                         "missing pre/post IR files")
            if not (pre.is_file() and post.is_file()):
                reporter.record("sccp", name, requirements, False,
                                checks.detail())
                continue

            pre_text = pre.read_text(encoding="latin-1")
            post_text = post.read_text(encoding="latin-1")
            _verify(ctx, post, checks, "post-SCCP")
            checks.check("alloca" not in post_text,
                         "post-SCCP IR still contains stack allocas")

            # Reconstruct the post-mem2reg pre-SCCP module (G-010 aid).
            pre_ssa = work / "pre-mem2reg.ll"
            proc = ctx.run([ctx.opt, "-passes=mem2reg", "-S", str(pre),
                            "-o", str(pre_ssa)], timeout=60)
            checks.check(proc.returncode == 0,
                         "external mem2reg failed: %s"
                         % proc.stderr.strip()[:200])
            pre_ssa_text = (pre_ssa.read_text(encoding="latin-1")
                            if pre_ssa.is_file() else "")

            for needle in fixture.get("pre", []):
                checks.check(needle in pre_text,
                             "pre-SCCP IR lacks %r" % needle)
            for needle in fixture.get("pre_mem2reg", []):
                checks.check(needle in pre_ssa_text,
                             "post-mem2reg pre-SCCP IR lacks %r" % needle)
            for needle in fixture.get("post", []):
                checks.check(needle in post_text,
                             "post-SCCP IR lacks %r" % needle)
            for needle in fixture.get("post_absent", []):
                checks.check(needle not in post_text,
                             "post-SCCP IR still contains %r" % needle)

            # Behaviour preservation: --compile and --optimize executables
            # must agree with each other and with the native reference.
            pre_run = _link_and_run(ctx, pre, plain_dir, name + "-plain",
                                    checks)
            post_run = _link_and_run(ctx, post, opt_dir, name + "-opt",
                                     checks)
            native = work / (name + "-native")
            native_compile = ctx.run(
                [ctx.clang, "-std=c11", "-Wno-everything",
                 str(work / (name + ".c")), "-o", str(native)],
                cwd=work, timeout=120)
            checks.check(native_compile.returncode == 0,
                         "native compile failed: %s"
                         % native_compile.stderr.strip()[:200])
            native_run = (ctx.run([native], cwd=work, timeout=20)
                          if native_compile.returncode == 0 else None)
            expected_exit = int(fixture["exit"])
            if pre_run is not None:
                checks.check(pre_run.returncode == expected_exit,
                             "--compile exit %d, expected %d"
                             % (pre_run.returncode, expected_exit))
            if post_run is not None:
                checks.check(post_run.returncode == expected_exit,
                             "--optimize exit %d, expected %d"
                             % (post_run.returncode, expected_exit))
            if pre_run is not None and post_run is not None:
                checks.check(pre_run.returncode == post_run.returncode,
                             "optimized exit %d differs from unoptimized %d"
                             % (post_run.returncode, pre_run.returncode))
                checks.check(pre_run.stdout == post_run.stdout,
                             "optimized stdout differs from unoptimized")
            if native_run is not None and post_run is not None:
                checks.check(post_run.returncode == native_run.returncode,
                             "optimized exit %d differs from native %d"
                             % (post_run.returncode, native_run.returncode))
                checks.check(post_run.stdout == native_run.stdout,
                             "optimized stdout differs from native")
            pairs += 1
            reporter.record("sccp", name, requirements, checks.ok,
                            checks.detail())
        finally:
            ctx.cleanup(work)

    reporter.check("sccp", "pre-post-pairs", ["G-010"], pairs == len(FIXTURES),
                   "%d/%d pre/post pairs captured" % (pairs, len(FIXTURES)))
