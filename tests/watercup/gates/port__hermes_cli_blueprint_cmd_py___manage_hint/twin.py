"""twin for hermes_cli/blueprint_cmd.py:_manage_hint (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/blueprint_cmd.py"
    PY_FN = "_manage_hint"
    C_FN = "blueprint_cmd_manage_hint"

    def norm(self, c, result):
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
