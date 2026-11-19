"""twin for tools/bot_mode_dm.py:_hermes_root (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/bot_mode_dm.py"
    PY_FN = "_hermes_root"
    C_FN = "_hermes_root"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
