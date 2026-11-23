#!/usr/bin/env python3
"""Differential oracle for w14_methodspro_1624be (tui_gateway/methods_prompt.py)."""
import importlib.util
import json
import sys
import types

try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except Exception:
    pass

Q = "C:/Users/eman5/hermes-agent"
S = Q + "/tui_gateway/methods_prompt.py"
if Q not in sys.path:
    sys.path.insert(0, Q)
if "tui_gateway" not in sys.modules:
    _pkg = types.ModuleType("tui_gateway")
    _pkg.__path__ = [Q + "/tui_gateway"]
    sys.modules["tui_gateway"] = _pkg
_mc = types.ModuleType("tui_gateway.method_ctx")
class _HR:
    def method(self, name=None):
        def d(fn): return fn
        return d
    def profile_scoped(self, fn): return fn
_mc.HandlerRegistry = _HR
_mc.bind_module = lambda *a, **kw: None
sys.modules["tui_gateway.method_ctx"] = _mc

spec = importlib.util.spec_from_file_location("tui_gateway.methods_prompt_w14", S)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)
live._err = lambda rid, code, message, data=None: {"code": code, "error": message}

def shape_range(t):
    a, b, e = t
    return {"first": a, "last": b, "err": e}

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line: continue
    try: r = json.loads(line)
    except Exception: continue
    func = r.get("func")
    try:
        if func == "message_row_id":
            py = live._message_row_id(r.get("msg") or {})
        elif func == "row_ids_of":
            py = sorted(live._row_ids_of(r.get("messages") or []))
        elif func == "pdf_page_range":
            live._PDF_ATTACH_MAX_PAGES = int(r.get("max_pages", 20))
            py = shape_range(live._pdf_page_range("rid", dict(r.get("params") or {})))
        elif func == "preview_restart_prompt":
            live._PREVIEW_RESTART_HISTORY_NOTE = r.get("note", "")
            live._PREVIEW_RESTART_RULES = tuple(r.get("rules") or [])
            py = live._preview_restart_prompt(r.get("url", ""), r.get("cwd", ""),
                                              r.get("context", ""), bool(r.get("has_history")))
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} {r.get('case')}: {exc!r}", file=sys.stderr)
        mism += 1; continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w14_methodspro_1624be: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
