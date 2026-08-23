"""twin for verification_stop_status_detail — LIVE agent/verification_stop.py::_status_detail."""
import os, sys, json
import importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

from watercup_py import Cup


def _load_live():
    try:
        spec = ilu.spec_from_file_location(
            "wc_live_vstop", os.path.join(DEV, "agent", "verification_stop.py"))
        mod = ilu.module_from_spec(spec)
        sys.modules["wc_live_vstop"] = mod
        spec.loader.exec_module(mod)
        return mod._status_detail
    except Exception:
        return None


_live_fn = _load_live()


def _fallback(status):
    state = str(status.get("status") or "unverified")
    evidence = status.get("evidence") if isinstance(status.get("evidence"), dict) else None
    if not evidence:
        return state
    command = evidence.get("canonical_command") or evidence.get("command")
    summary = str(evidence.get("output_summary") or "").strip()
    parts = [state]
    if command:
        parts.append(f"last command `{command}`")
    if summary:
        max_summary = 1200
        if len(summary) > max_summary:
            summary = summary[:max_summary].rstrip() + "\n... [truncated]"
        parts.append(f"last output:\n{summary}")
    return "\n".join(parts)


def trace(raw):
    try:
        status = json.loads(raw)
    except Exception:
        status = None

    c = Cup("_status_detail")
    obj = isinstance(status, dict)
    c.branch(obj)                                               # B1
    ev = status.get("evidence") if obj else None
    has_ev = isinstance(ev, dict) if obj else False
    c.branch(bool(has_ev))                                      # B2
    if has_ev:
        cmd = ev.get("canonical_command") or ev.get("command")
        c.branch(bool(cmd))                                     # B3
        c.branch(bool(str(ev.get("output_summary") or "").strip()))  # B4

    result = (_live_fn(status) if (_live_fn and obj) else
              (_fallback(status) if obj else "unverified"))
    c.valstr(result)
    return c.finalize()


def run(inputs):
    return [trace(r) for r in inputs]
