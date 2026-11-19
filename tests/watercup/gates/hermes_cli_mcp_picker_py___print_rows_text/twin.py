"""twin for hermes_cli/mcp_picker.py:_print_rows_text (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/mcp_picker.py"
    PY_FN = "_print_rows_text"
    C_FN = "hermes_cli_mcp_picker_u_print_rows_text"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
