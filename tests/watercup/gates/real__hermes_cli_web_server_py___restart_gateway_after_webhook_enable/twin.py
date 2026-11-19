"""twin for hermes_cli/web_server.py:_restart_gateway_after_webhook_enable (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/web_server.py"
    PY_FN = "_restart_gateway_after_webhook_enable"
    C_FN = "_restart_gateway_after_webhook_enable"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
