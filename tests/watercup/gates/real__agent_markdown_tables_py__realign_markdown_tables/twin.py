"""twin for agent/markdown_tables.py:realign_markdown_tables (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/markdown_tables.py"
    PY_FN = "realign_markdown_tables"
    C_FN = "realign_markdown_tables"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
