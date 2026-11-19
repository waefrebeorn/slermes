"""twin for tools/hook_output_spill.py:_build_preview (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/hook_output_spill.py"
    PY_FN = "_build_preview"
    C_FN = "tools_hook_output_spill_u_build_preview"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
