"""twin for tools/computer_use/cua_backend.py:_parse_elements_from_tree (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/computer_use/cua_backend.py"
    PY_FN = "_parse_elements_from_tree"
    C_FN = "cu_action_free"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
