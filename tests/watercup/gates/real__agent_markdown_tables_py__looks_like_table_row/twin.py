"""twin for agent/markdown_tables.py:looks_like_table_row (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/markdown_tables.py"
    PY_FN = "looks_like_table_row"
    C_FN = "mdt_looks_like_table_row"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
