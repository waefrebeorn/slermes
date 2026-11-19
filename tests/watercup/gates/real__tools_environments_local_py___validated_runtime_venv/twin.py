"""twin for tools/environments/local.py:_validated_runtime_venv (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/environments/local.py"
    PY_FN = "_validated_runtime_venv"
    C_FN = "_validated_runtime_venv"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
