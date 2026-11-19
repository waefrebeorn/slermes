"""twin for tools/discord_tool.py:_detect_capabilities_nonblocking (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/discord_tool.py"
    PY_FN = "_detect_capabilities_nonblocking"
    C_FN = "discord_detect_capabilities_nonblocking"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
