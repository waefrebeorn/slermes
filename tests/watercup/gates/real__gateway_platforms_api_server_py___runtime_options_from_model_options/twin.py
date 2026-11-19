"""twin for gateway/platforms/api_server.py:_runtime_options_from_model_options (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/api_server.py"
    PY_FN = "_runtime_options_from_model_options"
    C_FN = "_runtime_options_from_model_options"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
