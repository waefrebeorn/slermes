"""twin for tools/discord_tool.py:get_dynamic_schema (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/discord_tool.py"
    PY_FN = "get_dynamic_schema"
    C_FN = "get_dynamic_schema"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
