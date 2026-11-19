"""twin for tools/computer_use/vision_routing.py:_explicit_aux_vision_override (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/computer_use/vision_routing.py"
    PY_FN = "_explicit_aux_vision_override"
    C_FN = "tools_vision_aux_override"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
