"""twin for agent/anthropic_adapter.py:_apply_oauth_prose_aliases (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/anthropic_adapter.py"
    PY_FN = "_apply_oauth_prose_aliases"
    C_FN = "_apply_oauth_prose_aliases"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
