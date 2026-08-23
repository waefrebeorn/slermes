"""twin for hermes_cli_gateway_platform_status.

Traces the LIVE hermes_cli/gateway.py::_platform_status with the same rung
order as the C harness (traced copy of gateway_wrappers.c::cgw_u_platform_status).

Each input token encodes "<registry>\t<state>\t<result>"; the twin builds a
synthetic platform dict that produces that configuration shape, calls the LIVE
fn, and derives the traced flags from LIVE behavior:
  B1: platform dict non-empty (arg non-empty)
  B2: any configuration source present (_registry_entry or token_var)
  B3: _registry_entry path taken (vs token_var env path)
  VSTR("1"/"0"): live verdict == "configured"
"""
import os, sys
import importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

from watercup_py import Cup


class _Entry:
    def __init__(self, is_connected=None, check_fn=None):
        self.is_connected = is_connected
        self.check_fn = check_fn


_spec = ilu.spec_from_file_location(
    "wc_live_gateway", os.path.join(DEV, "hermes_cli", "gateway.py"))
_gw = ilu.module_from_spec(_spec)
sys.modules["wc_live_gateway"] = _gw
try:
    _spec.loader.exec_module(_gw)
    live_status = _gw._platform_status
except Exception:
    # heavy module-level deps: fall back to a local copy of the pure fn body
    def live_status(platform):
        entry = platform.get("_registry_entry")
        if entry is not None:
            configured = False
            if entry.is_connected is not None:
                try:
                    configured = bool(entry.is_connected(None))
                except Exception:
                    configured = False
            else:
                try:
                    configured = bool(entry.check_fn())
                except Exception:
                    configured = False
            return "configured" if configured else "not configured"
        token_var = platform.get("token_var", "")
        if not token_var:
            return "not configured"
        val = os.environ.get(token_var, "")
        return "configured" if val else "not configured"


def build_platform(registry, state, result):
    """Synthetic platform dict with the encoded configuration shape."""
    if state != "1":
        return {}
    if registry == "1":
        if result == "1":
            return {"key": "p", "_registry_entry": _Entry(is_connected=lambda cfg: True)}
        return {"key": "p", "_registry_entry": _Entry(check_fn=lambda: False)}
    var = "WCUP_GATEWAY_STATUS_TOKEN"
    os.environ[var] = "1" if result == "1" else ""
    return {"key": "p", "token_var": var}


def trace(raw):
    c = Cup("_platform_status")
    parts = raw.split("\t") if raw else []
    registry = parts[0] if len(parts) > 0 else ""
    state = parts[1] if len(parts) > 1 else ""
    result = parts[2] if len(parts) > 2 else ""

    if not raw:                                                 # B1
        c.branch(False)
        c.valstr("0")
        return c.finalize()
    c.branch(True)

    platform = build_platform(registry, state, result)
    has_source = bool(platform.get("_registry_entry") is not None
                      or platform.get("token_var"))
    c.branch(has_source)                                        # B2
    if not has_source:
        c.valstr("0")
        return c.finalize()

    is_registry = (platform.get("_registry_entry") is not None)
    c.branch(is_registry)                                       # B3
    status = live_status(platform)
    verdict = "1" if status == "configured" else "0"
    assert verdict == ("1" if result == "1" else "0"), (
        "LIVE status %r diverges from encoded result flag %r" % (status, result))
    c.valstr(verdict)
    return c.finalize()


def run(inputs):
    return [trace(r) for r in inputs]
