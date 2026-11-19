"""twin for hermes_cli/memory_setup.py:_clear_interactive_transition (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/memory_setup.py"
    PY_FN = "_clear_interactive_transition"
    C_FN = "_clear_interactive_transition"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
