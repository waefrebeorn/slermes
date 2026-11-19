"""twin for tools/bot_mode_dm.py:_unlink_dm_file (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/bot_mode_dm.py"
    PY_FN = "_unlink_dm_file"
    C_FN = "bot_mode_dm_unlink_dm_file"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
