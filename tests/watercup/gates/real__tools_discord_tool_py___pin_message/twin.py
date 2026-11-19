"""twin for tools/discord_tool.py:_pin_message (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/discord_tool.py"
    PY_FN = "_pin_message"
    C_FN = "_pin_message"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
