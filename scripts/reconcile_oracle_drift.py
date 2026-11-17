#!/usr/bin/env python3
"""Reconcile the drift allowlist by measuring it, not by parsing its summary.

verify_oracle_drift.py prints only findings that are NOT already allowlisted, so
the output can never tell you which entries are dead. Run it once with an empty
ALLOW to get the true live set, then add what is new and drop what is gone.
"""
from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path

REPO = Path("C:/Users/eman5/slermes")
ALLOW = REPO / "tests" / "oracle_drift_allowlist.py"
GATE = REPO / "tests" / "verify_oracle_drift.py"
REASON = "upstream helper deleted; the C port still implements it"

src = ALLOW.read_text(encoding="utf-8")
have = set(re.findall(r'"(sta_oracle_[\w]+\.py:\d+)":', src))
print("entries before:", len(have))

# `--dump` prints every finding, uncapped and unfiltered: reconciliation needs
# the true live set, and the normal report prints only NEW findings.
r = subprocess.run([sys.executable, str(GATE), "--dump"], cwd=str(REPO),
                   capture_output=True, text=True)
out = r.stdout + r.stderr
live = set(re.findall(r"^\s+(sta_oracle_[\w]+\.py):(\d+)\s+\S", out, re.M))
live = {f"{f}:{ln}" for f, ln in live}
print("live findings:", len(live))

dead = sorted(have - live)
add = sorted(live - have)
print("dead:", len(dead), "new:", len(add))

new_src = src
for k in dead:
    new_src = re.sub(r'^\s*"%s": .*\n' % re.escape(k), "", new_src, flags=re.M)
if add:
    m = re.search(r"^ALLOW = \{\n", new_src, re.M)
    assert m, "ALLOW = { not found"
    new_src = (new_src[:m.end()]
               + "".join('    "%s": "%s",\n' % (k, REASON) for k in add)
               + new_src[m.end():])

ALLOW.write_bytes(new_src.encode("utf-8"))
print("entries after:", len(re.findall(r'"(sta_oracle_[\w]+\.py:\d+)":',
                                       ALLOW.read_text(encoding="utf-8"))))
