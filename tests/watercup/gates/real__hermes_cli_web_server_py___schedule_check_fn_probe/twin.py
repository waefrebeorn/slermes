"""twin for hermes_cli/web_server.py:_schedule_check_fn_probe (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/web_server.py"
    PY_FN = "_schedule_check_fn_probe"
    C_FN = "_schedule_check_fn_probe"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
