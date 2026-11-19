"""twin for hermes_cli/pt_input_extras.py:_clear_vt100_prefix_cache (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/pt_input_extras.py"
    PY_FN = "_clear_vt100_prefix_cache"
    C_FN = "_clear_vt100_prefix_cache"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
