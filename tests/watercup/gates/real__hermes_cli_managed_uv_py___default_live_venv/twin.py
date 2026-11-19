"""twin for hermes_cli/managed_uv.py:_default_live_venv (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/managed_uv.py"
    PY_FN = "_default_live_venv"
    C_FN = "_default_live_venv"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
