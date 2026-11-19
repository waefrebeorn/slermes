"""twin for hermes_cli/session_listing.py:format_gateway_session_listing (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/session_listing.py"
    PY_FN = "format_gateway_session_listing"
    C_FN = "format_gateway_session_listing"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
