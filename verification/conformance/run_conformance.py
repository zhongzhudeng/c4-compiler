#!/usr/bin/env python3
"""Run the C4 assignment-conformance suite.

Usage:
    python3 verification/conformance/run_conformance.py --c4 build/c4
    python3 verification/conformance/run_conformance.py --suite lexer,parser
    python3 verification/conformance/run_conformance.py --update-requirements

The harness compares the real compiler with the independent reference
frontend in c4ref.  Results are written as JSON and summarised on stdout.
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from collections import defaultdict
from datetime import datetime, timezone
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
sys.path.insert(0, str(HERE))

from suite.runner import Context, Reporter, SUITE_NAMES  # noqa: E402


def find_c4(root: Path) -> Path:
    for candidate in (root / "build" / "c4",
                      root / "build" / "debug" / "c4",
                      root / "build" / "release" / "c4"):
        if candidate.is_file():
            return candidate
    raise SystemExit("cannot find the c4 binary; pass --c4")


def git_commit(root: Path) -> str:
    try:
        return subprocess.run(
            ["git", "-C", str(root), "rev-parse", "HEAD"],
            capture_output=True, text=True, check=True).stdout.strip()
    except Exception:
        return "unknown"


def git_dirty(root: Path) -> bool:
    try:
        result = subprocess.run(
            ["git", "-C", str(root), "status", "--porcelain"],
            capture_output=True, text=True, check=True)
        generated = ("verification/conformance/results/",
                     "verification/COVERAGE.md")
        changes = [line for line in result.stdout.splitlines()
                   if not any(path in line for path in generated)]
        return bool(changes)
    except Exception:
        return False


def aggregate(results):
    by_requirement = defaultdict(lambda: {"cases": 0, "failed": 0,
                                          "skipped": 0, "passed": 0,
                                          "names": []})
    for result in results:
        for requirement in result.requirements:
            entry = by_requirement[requirement]
            entry["cases"] += 1
            entry["names"].append(result.suite + "/" + result.case)
            if result.skipped:
                entry["skipped"] += 1
            elif result.passed:
                entry["passed"] += 1
            else:
                entry["failed"] += 1
    return dict(sorted(by_requirement.items()))


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--c4", type=Path, default=None)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--suite", default="all",
                        help="comma-separated suite names, or 'all'")
    parser.add_argument("--json", type=Path,
                        default=HERE / "results" / "evidence.json")
    parser.add_argument("--report", type=Path, default=ROOT / "verification" /
                        "COVERAGE.md")
    parser.add_argument("--update-requirements", action="store_true",
                        help="write statuses/evidence into requirements.yaml")
    parser.add_argument("--keep", action="store_true",
                        help="keep temporary files")
    parser.add_argument("--verbose", action="store_true")
    args = parser.parse_args(argv)

    root = args.root.resolve()
    c4 = (args.c4 or find_c4(root)).resolve()
    if not c4.is_file():
        raise SystemExit("c4 binary not found: %s" % c4)

    if args.suite == "all":
        selected = list(SUITE_NAMES)
    else:
        selected = [s.strip() for s in args.suite.split(",") if s.strip()]
        unknown = [s for s in selected if s not in SUITE_NAMES]
        if unknown:
            raise SystemExit("unknown suite(s): %s" % ", ".join(unknown))

    context = Context(c4, root, keep=args.keep)
    reporter = Reporter(verbose=args.verbose)

    # Imported lazily so a missing optional suite does not break others.
    from suite import (build_cases, cli_cases, lexer_cases, parser_cases,
                       sema_cases, printer_cases, codegen_cases, sccp_cases)
    modules = {
        "build": build_cases.run_build,
        "cli": cli_cases.run_cli,
        "lexer": lexer_cases.run_lexer,
        "parser": parser_cases.run_parser,
        "sema": sema_cases.run_sema,
        "printer": printer_cases.run_printer,
        "codegen": codegen_cases.run_codegen,
        "sccp": sccp_cases.run_sccp,
    }

    for name in selected:
        print("== suite %s ==" % name)
        modules[name](context, reporter)

    results = reporter.results
    passed = sum(1 for r in results if r.passed and not r.skipped)
    failed = sum(1 for r in results if not r.passed and not r.skipped)
    skipped = sum(1 for r in results if r.skipped)
    evidence = {
        "schema": 1,
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "c4": context.rel(c4),
        "c4_commit": git_commit(root),
        "working_tree_dirty": git_dirty(root),
        "root": ".",
        "suites": selected,
        "summary": {"cases": len(results), "passed": passed,
                    "failed": failed, "skipped": skipped},
        "results": [
            {"suite": r.suite, "case": r.case,
             "requirements": list(r.requirements),
             "status": ("skipped" if r.skipped else
                        ("pass" if r.passed else "fail")),
             "detail": r.detail}
            for r in results
        ],
        "requirements": aggregate(results),
    }
    args.json.parent.mkdir(parents=True, exist_ok=True)
    args.json.write_text(json.dumps(evidence, indent=2) + "\n")
    print("evidence: %s" % args.json)
    print("cases: %d passed, %d failed, %d skipped"
          % (passed, failed, skipped))

    if args.report:
        from suite.report import write_report
        write_report(args.report, evidence, root)

    if args.update_requirements:
        from suite.requirements import update_requirements
        update_requirements(root / "verification" / "spec" /
                            "requirements.yaml", evidence)

    return 0 if failed == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
