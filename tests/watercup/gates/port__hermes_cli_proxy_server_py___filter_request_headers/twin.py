"""twin for hermes_cli/proxy/server.py:_filter_request_headers (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/proxy/server.py"
    PY_FN = "_filter_request_headers"
    C_FN = "hermes_cli_proxy_server_u_filter_request_headers"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
