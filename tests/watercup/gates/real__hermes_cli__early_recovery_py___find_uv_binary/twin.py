"""twin for hermes_cli/_early_recovery.py:_find_uv_binary (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/_early_recovery.py"
    PY_FN = "_find_uv_binary"
    C_FN = "_find_uv_binary"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
