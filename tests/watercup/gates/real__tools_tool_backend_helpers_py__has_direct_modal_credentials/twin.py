"""twin for tools/tool_backend_helpers.py:has_direct_modal_credentials (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/tool_backend_helpers.py"
    PY_FN = "has_direct_modal_credentials"
    C_FN = "tools_tool_backend_helpers_has_direct_modal_credentials"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
