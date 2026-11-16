#!/usr/bin/env python3
"""Build a --batch spec for _oracle_boot.py from a TSV written by the runner.

Input lines:  case<TAB>fixture<TAB>c_output<TAB>extra
where `extra` is a space-separated argument list (may be empty). Keeping the
wire format TSV means the runner never has to build JSON in shell.

Usage: make_batch_spec.py <tsv> <oracle_path> <out_dir>
Writes <out_dir>/batch_<name>.json and prints its path.
"""
import json
import sys
from pathlib import Path

tsv, oracle, out_dir = sys.argv[1], sys.argv[2], Path(sys.argv[3])
name = Path(tsv).stem[len("batch_"):] if Path(tsv).stem.startswith("batch_") else Path(tsv).stem
cases = []
for line in Path(tsv).read_text(encoding="utf-8").splitlines():
    if not line.strip():
        continue
    parts = line.split("\t")
    case, fixture, c_output = parts[0], parts[1], parts[2]
    extra = parts[3].split() if len(parts) > 3 and parts[3].strip() else []
    cases.append({
        "name": case,
        "fixture": fixture,
        "stdin": c_output,
        "out": str(out_dir / f"oracle_{name}_py_{case}.json"),
        "extra": extra,
    })

out = out_dir / (Path(tsv).stem + ".json")
out.write_text(json.dumps({"oracle": oracle, "cases": cases}), encoding="utf-8")
print(out)
