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
stem = Path(tsv).stem
name = stem[len("batch_"):] if stem.startswith("batch_") else stem

# Normalisation config, read here rather than by three `python -c` spawns in
# the shell. The spec then carries everything the single batch interpreter
# needs to normalise AND compare, so the runner spawns nothing per case.
cfg = {"strip_patterns": [], "strip_fields": [], "float_int_equal": False}
try:
    reg = json.loads(Path("tests/oracle/registry.json").read_text(encoding="utf-8"))
    norm = reg.get("normalize", {}) or {}
    cfg["strip_patterns"] = list(norm.get("strip_patterns", []) or [])
    cfg["strip_fields"] = list(norm.get("strip_fields", []) or [])
    port = (reg.get("ports", {}) or {}).get(name)
    if isinstance(port, dict):
        cfg["strip_patterns"] += list(port.get("strip_patterns", []) or [])
        pcfg = port.get("normalize", {}) or {}
        cfg["float_int_equal"] = bool(pcfg.get("float_int_equal"))
        cfg["strip_fields"] += list(pcfg.get("strip_fields", []) or [])
except Exception:
    pass

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
        # CONTRACT: the oracle's stdin is the C program's OUTPUT -- the oracle
        # is a differential comparator, it must read what the port produced.
        # The fixture goes on argv, where an oracle that needs its input reads
        # it. Feeding the fixture to stdin instead silently breaks every port
        # whose harness emits its own cases (config_nested's fixture is literally
        # a placeholder saying so) and produced 9 JSONDecodeError ports.
        "stdin": c_output,
        "c_out": c_output,
        "out": (out_dir / f"oracle_{name}_py_{case}.json").as_posix(),
        "extra": extra,
    })

out = out_dir / (Path(tsv).stem + ".json")
out.write_text(json.dumps({"oracle": oracle, "cfg": cfg, "cases": cases}), encoding="utf-8")
print(out)
