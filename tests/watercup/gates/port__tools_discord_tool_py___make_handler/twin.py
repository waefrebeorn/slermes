"""twin for tools/discord_tool.py:_make_handler (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/discord_tool.py"
    PY_FN = "_make_handler"
    C_FN = "discord_make_handler"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
