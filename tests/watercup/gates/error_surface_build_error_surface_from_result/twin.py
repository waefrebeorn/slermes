"""twin for error_surface_build_error_surface_from_result.

Traces the LIVE agent/error_surface.py::build_error_surface_from_result with
the same rung order as the C harness. Uses LIVE module constants
(_REASON_TO_LAYER, _TRANSPORT_REASONS, _NON_RETRYABLE_REASONS,
_CUSTOM_ENDPOINT_PROVIDERS, _STREAM_DROP_FRAGMENTS) and LIVE
hermes_state.is_disk_full_error so the values are live-anchored.
Provider/model fixed to "" on both sides.
"""
import os
import sys
import json as _json
import importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup


def _load(name, relpath):
    spec = ilu.spec_from_file_location(name, os.path.join(DEV, relpath))
    mod = ilu.module_from_spec(spec)
    sys.modules[name] = mod
    spec.loader.exec_module(mod)
    return mod


_es = _load("wc_live_error_surface", "agent/error_surface.py")
_hs = _load("wc_live_hermes_state", "hermes_state.py")


def trace(raw):
    c = Cup("build_error_surface_from_result")
    try:
        result = _json.loads(raw)
    except Exception:
        result = None
    if not isinstance(result, dict):                       # B1
        c.branch(True)
        c.valstr("none")
        return c
    c.branch(False)
    error_text = str(result.get("error") or "")
    reason = str(result.get("failure_reason") or "").strip()
    c.valstr(error_text)
    c.valstr(reason)
    if not error_text and not reason:                      # B2
        c.branch(True)
        c.valstr("none")
        return c
    c.branch(False)
    if error_text and _hs.is_disk_full_error(error_text):  # B3
        c.branch(True)
        c.valstr("disk|disk_full|False")
        return c
    c.branch(False)
    billing_block = bool(result.get("billing_block"))
    if billing_block or reason in ("billing", "billing_unverified"):   # B4
        c.branch(True)
        c.valstr("billing|%s|False" % (reason or "billing"))
        return c
    c.branch(False)
    if not reason:                                         # B5
        c.branch(True)
        if _es._looks_like_stream_drop(error_text):         # B5a
            c.branch(True)
            c.valstr("streaming|stream_drop|True")
        else:
            c.branch(False)
            c.valstr("provider|unknown|True")
        return c
    c.branch(False)

    layer = _es._REASON_TO_LAYER.get(reason)
    mapped = layer is not None
    c.branch(mapped)                                       # B6
    if not mapped:
        if reason in _es._TRANSPORT_REASONS and _es._is_custom_endpoint(""):  # B7
            c.branch(True)
            layer = "endpoint"
        else:
            c.branch(False)
            if _es._looks_like_stream_drop(error_text):     # B8
                c.branch(True)
                layer = "streaming"
            else:
                c.branch(False)
                layer = "provider"

    retry_raw = result.get("failure_retryable")
    has_retry_bool = isinstance(retry_raw, bool)
    c.branch(has_retry_bool)                               # B9
    retryable = retry_raw if has_retry_bool else reason not in _es._NON_RETRYABLE_REASONS
    c.valstr("%s|%s|%s" % (layer, reason, "True" if retryable else "False"))
    return c


def run(inputs):
    return [trace(t).finalize() for t in inputs]
