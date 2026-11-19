"""twin for hermes_cli/web_server.py:get_model_info (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/web_server.py"
    PY_FN = "get_model_info"
    C_FN = "get_model_info"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
