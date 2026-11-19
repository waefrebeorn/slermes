"""twin for agent/tool_dispatch_helpers.py:_maybe_append_elision_notice (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/tool_dispatch_helpers.py"
    PY_FN = "_maybe_append_elision_notice"
    C_FN = "_maybe_append_elision_notice"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
