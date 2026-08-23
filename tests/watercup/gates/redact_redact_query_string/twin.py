"""twin for redact_redact_query_string — LIVE agent/redact.py::_redact_query_string."""
import os, sys
import importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

from watercup_py import Cup

_SENSITIVE_QUERY_PARAMS = frozenset({
    "access_token", "refresh_token", "id_token", "token", "api_key", "apikey",
    "client_secret", "password", "auth", "jwt", "session", "secret", "key",
    "code", "signature", "x-amz-signature",
})


def _load_live():
    try:
        spec = ilu.spec_from_file_location(
            "wc_live_red", os.path.join(DEV, "agent", "redact.py"))
        mod = ilu.module_from_spec(spec)
        sys.modules["wc_live_red"] = mod
        spec.loader.exec_module(mod)
        return mod._redact_query_string
    except Exception:
        return None


_live_fn = _load_live()


def _fallback(query):
    if not query:
        return query
    parts = []
    for pair in query.split("&"):
        if "=" not in pair:
            parts.append(pair)
            continue
        key, _, value = pair.partition("=")
        if key.lower() in _SENSITIVE_QUERY_PARAMS:
            parts.append(f"{key}=***")
        else:
            parts.append(pair)
    return "&".join(parts)


def trace(raw):
    c = Cup("_redact_query_string")
    query = raw
    if not query:                       # LIVE early-returns before the loop
        result = _live_fn(query) if _live_fn else _fallback(query)
        c.valstr(result)
        return c.finalize()
    for pair in query.split("&"):
        has_eq = "=" in pair
        key = pair.partition("=")[0]
        sens = has_eq and key.lower() in _SENSITIVE_QUERY_PARAMS
        c.branch(has_eq)
        c.branch(sens)

    result = _live_fn(query) if _live_fn else _fallback(query)
    c.valstr(result)
    return c.finalize()


def run(inputs):
    return [trace(r) for r in inputs]
