#!/usr/bin/env python3
"""Differential oracle for agent/provider_base.py (w6_providerbas_bb26d7 gate)."""
import importlib.util, json, sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
PYROOT = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, PYROOT)
SRC = PYROOT + "/agent/provider_base.py"
spec = importlib.util.spec_from_file_location("live_provider_base", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["live_provider_base"] = live
spec.loader.exec_module(live)

class P(live.ProviderBase):
    def __init__(self, name): self._n = name
    @property
    def name(self): return self._n

class C(live.CatalogProviderBase):
    def __init__(self, name, models=None): self._n = name; self._m = models or []
    @property
    def name(self): return self._n
    def list_models(self): return self._m

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
    name = r.get("name", "")
    try:
        if func == "display_name":
            py = P(name).display_name
        elif func == "setup_schema":
            py = P(name).get_setup_schema()
        elif func == "catalog_display_name":
            py = C(name).display_name
        elif func == "catalog_is_available":
            py = C(name).is_available()
        elif func == "catalog_list_models":
            py = C(name).list_models()
        elif func == "catalog_default_model":
            py = C(name, r.get("models")).default_model()
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w6_providerbas_bb26d7: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
