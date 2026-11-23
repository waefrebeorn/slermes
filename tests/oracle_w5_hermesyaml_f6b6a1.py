#!/usr/bin/env python3
"""Oracle for upstream Hermes hermes_yaml.py (w5_hermesyaml_f6b6a1 gate)."""
import importlib.util, json, sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
PYROOT = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, PYROOT)
SRC = PYROOT + "/hermes_yaml.py"
spec = importlib.util.spec_from_file_location("hermesyaml_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["hermesyaml_live"] = live
spec.loader.exec_module(live)

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    func = r.get("func")
    if func == "version":
        py = list(live._Yaml11Resolver().processing_version)
    elif func == "width":
        py = live.ROUNDTRIP_YAML_WIDTH
    elif func == "dump":
        py = live.safe_dump(r.get("v"))
    else:
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w5_hermesyaml_f6b6a1: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
