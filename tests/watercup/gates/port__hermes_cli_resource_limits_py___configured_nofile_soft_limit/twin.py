"""twin for hermes_cli/resource_limits.py:_configured_nofile_soft_limit (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/resource_limits.py"
    PY_FN = "_configured_nofile_soft_limit"
    C_FN = "_configured_nofile_soft_limit"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
