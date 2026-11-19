"""twin for tools/computer_use/vision_routing.py:should_route_capture_to_aux_vision (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/computer_use/vision_routing.py"
    PY_FN = "should_route_capture_to_aux_vision"
    C_FN = "should_route_capture_to_aux_vision"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
