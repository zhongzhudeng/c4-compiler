"""Map executed conformance evidence onto requirements.yaml.

Status policy (see verification/COVERAGE.md):

* out-of-scope requirements are not-applicable;
* a requirement with no mapped case stays uncovered;
* a requirement with a failing or skipped case is partial;
* a passing requirement whose claim is necessarily corpus-scoped is partial;
* every other passing requirement is verified for the stated corpus.
"""

from __future__ import annotations

import json
import re
from pathlib import Path
from typing import Dict, Tuple

# These claims are universal over all programs.  The suite supplies
# counterexample-free evidence on a corpus, which is not a proof; the
# status therefore stays partial by construction.
CORPUS_SCOPED = {
    "A-001",  # acceptance boundary over all C programs
    "A-012",  # every invalid C program is rejected
    "D-008",  # round-trip stability over all printable programs
    "F-008",  # differential execution over all defined C4 programs
    "G-009",  # behavior preservation over all defined programs
    "L-014",  # asymptotic linear running time
}

ID_RE = re.compile(r"^- id: (\S+)\s*$")
STATUS_RE = re.compile(r"^  status: .*$")
EVIDENCE_RE = re.compile(r"^  evidence: .*$")


def load_requirements(path: Path):
    import yaml
    with path.open(encoding="utf-8") as handle:
        return yaml.safe_load(handle)["requirements"]


def requirement_status(requirement, evidence_entry) -> Tuple[str, str]:
    identifier = requirement["id"]
    classification = requirement.get("classification", "")
    if classification == "out-of-scope":
        return ("not-applicable",
                "out of scope: %s" % requirement.get("notes", "").strip())
    if not evidence_entry or evidence_entry["cases"] == 0:
        return "uncovered", "no conformance case mapped"
    cases = evidence_entry["cases"]
    failed = evidence_entry["failed"]
    skipped = evidence_entry["skipped"]
    names = ", ".join(evidence_entry["names"][:4])
    if len(evidence_entry["names"]) > 4:
        names += ", ..."
    summary = ("run_conformance.py: %d case(s), %d failed, %d skipped "
               "(%s)" % (cases, failed, skipped, names))
    if failed or skipped:
        return "partial", summary
    if identifier in CORPUS_SCOPED:
        return "partial", summary + "; corpus-scoped, not a universal proof"
    return "verified", summary


def compute_statuses(evidence, requirements):
    by_id = evidence.get("requirements", {})
    return {requirement["id"]: requirement_status(
        requirement, by_id.get(requirement["id"]))
        for requirement in requirements}


def update_requirements(path: Path, evidence):
    requirements = load_requirements(path)
    statuses = compute_statuses(evidence, requirements)
    lines = path.read_text(encoding="utf-8").splitlines()
    current = None
    seen = set()
    output = []
    for line in lines:
        match = ID_RE.match(line)
        if match:
            current = match.group(1)
            seen.add(current)
        if current and STATUS_RE.match(line):
            status = statuses.get(current, ("uncovered", ""))[0]
            output.append("  status: %s" % status)
            continue
        if current and EVIDENCE_RE.match(line):
            text = statuses.get(current, ("uncovered", ""))[1]
            output.append("  evidence: %s" % json.dumps(text))
            continue
        output.append(line)
    missing = sorted(set(statuses) - seen)
    if missing:
        raise SystemExit("requirements not found in YAML: %s"
                         % ", ".join(missing))
    path.write_text("\n".join(output) + "\n", encoding="utf-8")
    return statuses
