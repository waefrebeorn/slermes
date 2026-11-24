#!/usr/bin/env python3
"""Oracle for gate w16_slashcomma_e06e76 (gateway/slash_commands_model.py)."""
import importlib.util
import json
import sys
import types
import contextlib
import io

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, QUARRY)

# ruamel stub: hermes_yaml pulls it transitively; the pure tables here never use it.
ruamel = types.ModuleType("ruamel"); ruamel.__path__ = []
ry = types.ModuleType("ruamel.yaml"); ry.__path__ = []
class YAML:
    def __init__(self, *a, **k): pass
    def load(self, *a, **k): return None
    def dump(self, *a, **k): pass
    def indent(self, *a, **k): return self
ry.YAML = YAML
err = types.ModuleType("ruamel.yaml.error")
class YAMLError(Exception): pass
err.YAMLError = YAMLError
res = types.ModuleType("ruamel.yaml.resolver")
class VersionedResolver: pass
res.VersionedResolver = VersionedResolver
ry.error = err; ry.resolver = res; ruamel.yaml = ry
sys.modules.update({"ruamel": ruamel, "ruamel.yaml": ry,
                    "ruamel.yaml.error": err, "ruamel.yaml.resolver": res})

def _load(dotted, rel):
    parts = dotted.split(".")
    for i in range(1, len(parts)):
        pn = ".".join(parts[:i])
        if pn not in sys.modules:
            pkg = types.ModuleType(pn)
            pkg.__path__ = [QUARRY + "/" + "/".join(parts[:i])]
            sys.modules[pn] = pkg
    spec = importlib.util.spec_from_file_location(dotted, QUARRY + "/" + rel)
    m = importlib.util.module_from_spec(spec)
    sys.modules[dotted] = m
    with contextlib.redirect_stderr(io.StringIO()):
        spec.loader.exec_module(m)
    return m

live = _load("gateway.slash_commands_model", "gateway/slash_commands_model.py")

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    func = r.get("func")
    if func == "fast_selections_table":
        py = {k: list(v) for k, v in live._FAST_SELECTIONS.items()}
    elif func == "fast_selection":
        v = live._FAST_SELECTIONS.get(r.get("arg"))
        py = list(v) if v is not None else None
    elif func == "reasoning_toggles_table":
        py = dict(live._REASONING_DISPLAY_TOGGLES)
    elif func == "reasoning_toggle":
        py = live._REASONING_DISPLAY_TOGGLES.get(r.get("arg"))
    elif func == "text_listing_models":
        py = live._TEXT_LISTING_MODELS
    else:
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w16_slashcomma_e06e76: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
