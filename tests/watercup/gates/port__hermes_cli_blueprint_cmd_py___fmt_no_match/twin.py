"""twin for hermes_cli/blueprint_cmd.py:_fmt_no_match (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/blueprint_cmd.py"
    PY_FN = "_fmt_no_match"
    C_FN = "blueprint_cmd_format_no_match"

    def norm(self, c, result):
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
