"""twin for tools/computer_use/schema.py:get_computer_use_schema (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/computer_use/schema.py"
    PY_FN = "get_computer_use_schema"
    C_FN = "get_computer_use_schema"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
