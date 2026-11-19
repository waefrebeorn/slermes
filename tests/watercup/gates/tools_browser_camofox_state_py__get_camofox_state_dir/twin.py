"""twin for tools/browser_camofox_state.py:get_camofox_state_dir (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/browser_camofox_state.py"
    PY_FN = "get_camofox_state_dir"
    C_FN = "tools_browser_camofox_state_get_camofox_state_dir"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
