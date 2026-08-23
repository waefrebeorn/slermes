"""twin for chat_helpers_status_code_from_payload — LIVE
agent/chat_completion_helpers.py::_status_code_from_payload (+ _value)."""
import os, sys, json, re
import importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

from watercup_py import Cup


def _load_live():
    try:
        spec = ilu.spec_from_file_location(
            "wc_live_cch", os.path.join(DEV, "agent", "chat_completion_helpers.py"))
        mod = ilu.module_from_spec(spec)
        sys.modules["wc_live_cch"] = mod
        spec.loader.exec_module(mod)
        return mod._status_code_from_payload, mod._status_code_from_value
    except Exception:
        return None, None


_live_payload, _live_value = _load_live()

_WORD_RE = re.compile(r"(?:HTTP_STATUS/)?\b([1-5]\d\d)\b", re.IGNORECASE)


def _value(candidate):
    if isinstance(candidate, bool):  # bool is an int subclass; LIVE treats it as non-int? keep LIVE order
        pass
    if _live_value is not None:
        return _live_value(candidate)
    if isinstance(candidate, int) and not isinstance(candidate, bool) and 100 <= candidate < 600:
        return candidate
    if not isinstance(candidate, str):
        return None
    m = _WORD_RE.search(candidate)
    if not m:
        return None
    try:
        return int(m.group(1))
    except ValueError:
        return None


def _payload(payload):
    if _live_payload is not None:
        return _live_payload(payload)
    if not isinstance(payload, dict):
        return None
    candidates = [payload.get("status_code"), payload.get("status"),
                  payload.get("http_status")]
    error_obj = payload.get("error")
    if isinstance(error_obj, dict):
        candidates.extend([error_obj.get("status_code"), error_obj.get("status"),
                           error_obj.get("http_status"), error_obj.get("code")])
    candidates.append(payload.get("code"))
    for cand in candidates:
        sc = _value(cand)
        if sc is not None:
            return sc
    return None


def trace(raw):
    try:
        payload = json.loads(raw)
    except Exception:
        payload = None

    c = Cup("_status_code_from_payload")
    obj = isinstance(payload, dict)
    c.branch(obj)                                            # B1
    err = payload.get("error") if obj else None
    errobj = isinstance(err, dict)

    def slot(o, k):
        return _value(o.get(k)) is not None if isinstance(o, dict) else False

    for k in ("status_code", "status", "http_status"):       # B2-B4
        c.branch(slot(payload, k))
    for k in ("status_code", "status", "http_status", "code"):  # B5-B8
        c.branch(errobj and slot(err, k))
    c.branch(slot(payload, "code"))                          # B9

    result = _payload(payload)
    c.valstr("-1" if result is None else str(result))
    return c.finalize()


def run(inputs):
    return [trace(r) for r in inputs]
