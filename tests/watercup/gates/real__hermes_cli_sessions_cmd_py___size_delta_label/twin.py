"""twin for hermes_cli/sessions_cmd.py:_size_delta_label (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/sessions_cmd.py"
    PY_FN = "_size_delta_label"
    C_FN = "main_u_size_delta_label"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
