"""twin for hermes_cli/web_server.py:_forward_cron_fire_to_gateway (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/web_server.py"
    PY_FN = "_forward_cron_fire_to_gateway"
    C_FN = "_forward_cron_fire_to_gateway"

    def norm(self, c, result):
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
