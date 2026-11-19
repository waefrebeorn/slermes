"""twin for hermes_cli/web_server.py:_gateway_ws_ticket_from_subprotocol (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/web_server.py"
    PY_FN = "_gateway_ws_ticket_from_subprotocol"
    C_FN = "_gateway_ws_ticket_from_subprotocol"

    def norm(self, c, result):
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
