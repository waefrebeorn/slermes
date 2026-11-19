"""twin for tools/hook_output_spill.py:_coerce_non_negative_int (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/hook_output_spill.py"
    PY_FN = "_coerce_non_negative_int"
    C_FN = "tools_hook_output_spill_u_coerce_non_negative_int"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
