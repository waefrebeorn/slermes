"""twin for hermes_cli/blueprint_cmd.py:_humanize_schedule (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/blueprint_cmd.py"
    PY_FN = "_humanize_schedule"
    C_FN = "blueprint_cmd_humanize_schedule"

    def norm(self, c, result):
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
