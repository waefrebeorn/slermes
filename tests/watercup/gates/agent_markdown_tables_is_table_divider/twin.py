"""twin for agent/markdown_tables.py:is_table_divider (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/markdown_tables.py"
    PY_FN = "is_table_divider"
    C_FN = "mdt_is_table_divider"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
