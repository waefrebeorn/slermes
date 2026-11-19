"""twin for tools/desktop_ui.py:set_emitter (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/desktop_ui.py"
    PY_FN = "set_emitter"
    C_FN = "tools_desktop_ui_set_emitter"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
