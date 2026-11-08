"""Map executed conformance evidence onto requirements.yaml.

Status policy (see verification/COVERAGE.md):

* out-of-scope requirements are not-applicable;
* a requirement with no mapped case stays uncovered;
* a requirement with a failing, skipped, or blocked case is partial/blocked;
* a passing requirement is corpus-verified: the finite corpus conforms, but
  the result is not a proof for untested programs;
* only a machine-checked universal claim may be verified.  No current
  requirement has such a claim, so the set below is empty by construction.
"""

from __future__ import annotations

import json
import re
from pathlib import Path
from typing import Dict, Tuple

# Requirements backed by a machine-checked universal claim rather than a
# finite test corpus.  Keep this set empty until such a check exists: a
# passing finite corpus must never be promoted to "verified".
MACHINE_CHECKED_UNIVERSAL = set()

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
    blocked = evidence_entry.get("blocked", 0)
    names = ", ".join(evidence_entry["names"][:4])
    if len(evidence_entry["names"]) > 4:
        names += ", ..."
    summary = ("run_conformance.py: %d case(s), %d failed, %d skipped "
               "(%s)" % (cases, failed, skipped, names))
    if blocked:
        return "blocked", summary + "; %d blocked check(s)" % blocked
    if failed or skipped:
        return "partial", summary
    if identifier in MACHINE_CHECKED_UNIVERSAL:
        return "verified", summary + "; machine-checked universal claim"
    return ("corpus-verified",
            summary + "; finite corpus conformance, not a universal proof")


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
