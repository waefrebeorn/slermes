"""twin for gateway/relay/ws_transport.py:_env_disconnect_budget_s (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/relay/ws_transport.py"
    PY_FN = "_env_disconnect_budget_s"
    C_FN = "wst_env_disconnect_budget_s"

    def norm(self, c, result):
        c.valstr("%.17g" % float(result))


_t = _Twin()
trace = _t.trace
run = _t.run
