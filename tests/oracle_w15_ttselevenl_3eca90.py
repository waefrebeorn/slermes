#!/usr/bin/env python3
"""Differential oracle for tools/tts_elevenlabs_voices.py (gate w15_ttselevenl_3eca90)."""
import importlib.util
import json
import sys
import types

try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

QUARRY = "C:/Users/eman5/hermes-agent"
G = "w15_ttselevenl_3eca90"
sys.path.insert(0, QUARRY)


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


def _noop(*args, **kwargs):
    return None


# The four ported helpers never call the network; stub httpx + tools.registry.
_hx = _mod("httpx")
_hx.get = _noop
_hx.HTTPError = type("HTTPError", (Exception,), {})
_to = _mod("tools")
_to.__path__ = []
_mod("tools.registry", tool_error=lambda msg, success=True: {"success": success, "error": msg})

SRC = QUARRY + "/tools/tts_elevenlabs_voices.py"
_spec = importlib.util.spec_from_file_location("tts_elevenlabs_voices_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["tts_elevenlabs_voices_live"] = live
_spec.loader.exec_module(live)


def live_out(rec):
    func = rec.get("func")
    if func == "clip":
        return live._clip(rec.get("text"))
    if func == "voice_row":
        return live._voice_row(rec.get("voice") or {})
    if func == "request_params":
        try:
            v = live._request_params(rec.get("source", ""), rec.get("query", ""), rec.get("filters") or {},
                                     rec.get("use_case", ""), rec.get("sort", ""), rec.get("page", ""),
                                     int(rec.get("page_size", 20)))
            return list(v)
        except Exception as exc:
            return {"__raise__": type(exc).__name__, "msg": str(exc)}
    if func == "next_page":
        return live._next_page(rec.get("source", ""), rec.get("body") or {}, rec.get("page", ""))
    return None


def main():
    cases = mism = 0
    allowed = ("clip", "voice_row", "request_params", "next_page")
    for line in sys.stdin:
        line = line.strip()
        if not line or "{" not in line:
            continue
        try:
            rec = json.loads(line)
        except ValueError:
            continue
        if rec.get("func") not in allowed:
            continue
        cases += 1
        try:
            py = live_out(rec)
        except Exception as exc:
            py = {"__raise__": type(exc).__name__, "msg": str(exc)}
        got = rec.get("out")
        if py != got:
            mism += 1
            print(f"MISMATCH {G} case={rec.get('case')} C={got!r} PY={py!r}", file=sys.stderr)
    if cases == 0:
        print(f"ORACLE {G}: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
