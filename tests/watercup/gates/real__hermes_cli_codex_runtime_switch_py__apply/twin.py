"""twin for hermes_cli/codex_runtime_switch.py:apply (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/codex_runtime_switch.py"
    PY_FN = "apply"
    C_FN = "apply"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
