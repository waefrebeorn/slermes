"""twin for hermes_cli/session_listing.py:query_session_listing (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/session_listing.py"
    PY_FN = "query_session_listing"
    C_FN = "hermes_cli_session_listing_query_session_listing"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
