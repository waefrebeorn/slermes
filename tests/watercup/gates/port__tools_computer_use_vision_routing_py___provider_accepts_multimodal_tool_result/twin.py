"""twin for tools/computer_use/vision_routing.py:_provider_accepts_multimodal_tool_result (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/computer_use/vision_routing.py"
    PY_FN = "_provider_accepts_multimodal_tool_result"
    C_FN = "cli_tools_computer_use_vision_routing__provider_accepts_multimodal_tool_result"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
