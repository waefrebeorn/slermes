#!/usr/bin/env python3
"""Differential oracle for tools/tts_tool_providers.py (gate w11_ttstoolpr_0ad803)."""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, QUARRY)

# hermes_yaml (ruamel backend) is not installed here; stub it for the import chain.
class _Any:
    def __init__(self, n="Any"): self._n = n
    def __call__(self, *a, **k): return _Any(self._n + "()")
    def __getattr__(self, n): return _Any(self._n + "." + n)
    def __iter__(self): return iter(())
    def __len__(self): return 0
    def __bool__(self): return True

for nm in ("hermes_yaml", "ruamel"):
    mod = types.ModuleType(nm)
    mod.__path__ = []
    mod.__getattr__ = lambda n: _Any(nm + "." + n)
    sys.modules[nm] = mod

spec = importlib.util.spec_from_file_location("w11_ttstoolpr_live", QUARRY + "/tools/tts_tool_providers.py")
live = importlib.util.module_from_spec(spec)
sys.modules["w11_ttstoolpr_live"] = live
spec.loader.exec_module(live)

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
    got = r.get("out")
    try:
        if func == "config_bool":
            want = live._config_bool(r.get("value"), bool(r.get("default", False)))
        elif func == "response_format":
            want = live._tts_response_format_from_path(r.get("path", ""))
        elif func == "clamped_number":
            want = live._clamped_number(r.get("raw"),
                    float if r.get("cast", "float") == "float" else int,
                    r.get("lo", 0.0), r.get("hi", 1.0))
        elif func == "gemini_tags":
            want = live._gemini_audio_tags_enabled(r.get("gemini_config", {}), r.get("model", ""))
        elif func == "compose":
            want = live._compose_gemini_tts_prompt(r.get("text", ""), {},
                                                   persona_prompt=r.get("persona_prompt", ""))
        else:
            continue
    except BaseException as ex:
        print("MISMATCH %s oracle raised %s: %s" % (func, type(ex).__name__, ex), file=sys.stderr)
        cases += 1
        mism += 1
        continue
    cases += 1
    if want != got:
        mism += 1
        print("MISMATCH in=%s py=%r c=%r" % (json.dumps(r, ensure_ascii=False), want, got), file=sys.stderr)
print("ORACLE w11_ttstoolpr_0ad803: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or cases == 0 else 0)
