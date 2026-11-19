"""twin for tools/computer_use/vision_routing.py:_lookup_supports_vision (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/computer_use/vision_routing.py"
    PY_FN = "_lookup_supports_vision"
    C_FN = "tools_vision_lookup_supports"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
