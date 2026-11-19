"""twin for gateway/run.py:_ensure_windows_gateway_venv_imports (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/run.py"
    PY_FN = "_ensure_windows_gateway_venv_imports"
    C_FN = "gw_ensure_windows_gateway_venv_imports"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
