#!/usr/bin/env python3
"""End-to-end tests for the C4 compiler.

Each program under tests/e2e/programs carries directives in its leading
comments:

    // exit: 42              expected process exit status (default 0)
    // stdout: hello         expected stdout line (repeatable, newline added)
    // args: one two         arguments passed to the compiled program
    // modes: compile,optimize   modes to test (default: both)

For every program and every requested mode the harness runs c4 to produce
<name>.ll, links it with clang, executes it, and compares the result with the
directives.  It also compiles the original C source with clang and checks that
the reference program behaves identically.

Programs under tests/e2e/rejects are expected to be rejected:

    // error: substring      expected diagnostic message
    // line: 3               optional expected line number
"""

import argparse
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

DIRECTIVE = re.compile(
    r"^\s*//\s*(exit|stdout|args|modes|error|line)\s*:\s?(.*)$"
)
DIAGNOSTIC = re.compile(r":(\d+):(\d+): error: ")


def parse_directives(path):
    directives = {
        "exit": 0,
        "stdout": "",
        "args": [],
        "modes": ["compile", "optimize"],
        "error": None,
        "line": None,
    }
    stdout_lines = []
    for raw in path.read_text().splitlines():
        match = DIRECTIVE.match(raw)
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
            directives["modes"] = [m.strip() for m in value.split(",") if m.strip()]
        elif key == "error":
            directives["error"] = value
        elif key == "line":
            directives["line"] = int(value)
    directives["stdout"] = "".join(line + "\n" for line in stdout_lines)
    return directives


def run(command, cwd, timeout=120):
    try:
        return subprocess.run(
            command, cwd=cwd, capture_output=True, text=True, timeout=timeout
        )
    except subprocess.TimeoutExpired:
        return subprocess.CompletedProcess(
            command, 124, "", f"timed out after {timeout}s"
        )


class Reporter:
    def __init__(self, verbose):
        self.verbose = verbose
        self.failures = []
        self.checks = 0

    def ok(self, name):
        self.checks += 1
        if self.verbose:
            print(f"ok   {name}")

    def fail(self, name, message):
        self.checks += 1
        self.failures.append((name, message))
        print(f"FAIL {name}: {message}")

    def check(self, name, condition, message):
        if condition:
            self.ok(name)
        else:
            self.fail(name, message)


def check_program(reporter, c4, clang, path):
    directives = parse_directives(path)
    stem = path.stem
    for mode in directives["modes"]:
        name = f"{stem}[{mode}]"
        with tempfile.TemporaryDirectory(prefix="c4-e2e-") as tmp:
            source = Path(tmp) / f"{stem}.c"
            shutil.copyfile(path, source)

            result = run([c4, f"--{mode}", source.name], tmp)
            if result.returncode != 0:
                reporter.fail(
                    name,
                    f"c4 --{mode} failed with {result.returncode}: "
                    f"{result.stderr.strip()}",
                )
                continue
            reporter.ok(f"{name} compiles")

            ir = Path(tmp) / f"{stem}.ll"
            reporter.check(name, ir.exists(), f"missing output file {ir.name}")
            if not ir.exists():
                continue

            executable = Path(tmp) / "program"
            result = run(
                [clang, "-Wno-override-module", str(ir), "-o", str(executable)], tmp
            )
            if result.returncode != 0:
                reporter.fail(
                    name, f"clang rejected the generated IR: {result.stderr.strip()}"
                )
                continue
            reporter.ok(f"{name} links")

            printed = run([c4, "--print-ast", source.name], tmp)
            if printed.returncode != 0:
                reporter.fail(
                    f"{name} print",
                    f"c4 --print-ast failed: {printed.stderr.strip()}",
                )
            else:
                roundtrip = Path(tmp) / "roundtrip.c"
                roundtrip.write_text(printed.stdout)
                result = run([c4, "--parse", roundtrip.name], tmp)
                reporter.check(
                    f"{name} print/parse round trip",
                    result.returncode == 0,
                    f"printed program was rejected: {result.stderr.strip()}",
                )

            result = run([str(executable)] + directives["args"], tmp, timeout=10)
            reporter.check(
                f"{name} exit status",
                result.returncode == directives["exit"],
                f"expected exit {directives['exit']}, got {result.returncode}",
            )
            reporter.check(
                f"{name} stdout",
                result.stdout == directives["stdout"],
                f"expected stdout {directives['stdout']!r}, "
                f"got {result.stdout!r}",
            )

            # Differential check against the host C compiler.
            reference = Path(tmp) / "reference"
            result = run(
                [
                    clang,
                    "-std=c11",
                    "-Wno-everything",
                    str(source),
                    "-o",
                    str(reference),
                ],
                tmp,
            )
            if result.returncode != 0:
                reporter.fail(
                    name,
                    f"clang rejected the C4 source: {result.stderr.strip()}",
                )
                continue
            reference_result = run(
                [str(reference)] + directives["args"], tmp, timeout=10
            )
            reporter.check(
                f"{name} differential exit",
                reference_result.returncode == directives["exit"],
                f"reference exited {reference_result.returncode}, "
                f"expected {directives['exit']}",
            )
            reporter.check(
                f"{name} differential stdout",
                reference_result.stdout == directives["stdout"],
                f"reference stdout {reference_result.stdout!r}, "
                f"expected {directives['stdout']!r}",
            )


def check_reject(reporter, c4, path):
    directives = parse_directives(path)
    name = f"reject:{path.stem}"
    with tempfile.TemporaryDirectory(prefix="c4-reject-") as tmp:
        source = Path(tmp) / path.name
        shutil.copyfile(path, source)

        result = run([c4, "--compile", source.name], tmp)
        reporter.check(
            name, result.returncode == 1, f"expected exit 1, got {result.returncode}"
        )

        match = DIAGNOSTIC.search(result.stderr)
        reporter.check(
            name,
            match is not None,
            f"missing '<file>:<line>:<column>: error:' diagnostic: "
            f"{result.stderr!r}",
        )
        if match and directives["line"] is not None:
            reporter.check(
                name,
                int(match.group(1)) == directives["line"],
                f"expected error on line {directives['line']}, "
                f"got line {match.group(1)}",
            )
        if directives["error"]:
            reporter.check(
                name,
                directives["error"] in result.stderr,
                f"expected diagnostic containing {directives['error']!r}, "
                f"got {result.stderr!r}",
            )
        reporter.check(
            name,
            not (Path(tmp) / f"{path.stem}.ll").exists(),
            "rejected program produced an output file",
        )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--c4", required=True, help="path to the c4 binary")
    parser.add_argument("--clang", default="clang", help="C compiler for linking")
    parser.add_argument("--programs", required=True, help="accepted program directory")
    parser.add_argument("--rejects", help="rejected program directory")
    parser.add_argument("--verbose", action="store_true")
    args = parser.parse_args()

    c4 = str(Path(args.c4).resolve())
    clang = shutil.which(args.clang) or args.clang
    clang = str(Path(clang).resolve()) if Path(clang).exists() else clang

    programs = sorted(Path(args.programs).glob("*.c"))
    rejects = sorted(Path(args.rejects).glob("*.c")) if args.rejects else []
    if not programs:
        print(f"error: no programs found in {args.programs}", file=sys.stderr)
        return 2

    reporter = Reporter(args.verbose)
    for path in programs:
        check_program(reporter, c4, clang, path)
    for path in rejects:
        check_reject(reporter, c4, path)

    print(
        f"{len(programs)} programs, {len(rejects)} rejected programs, "
        f"{reporter.checks} checks, {len(reporter.failures)} failures"
    )
    for name, message in reporter.failures:
        print(f"  {name}: {message}")
    return 1 if reporter.failures else 0


if __name__ == "__main__":
    sys.exit(main())
