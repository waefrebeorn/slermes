"""twin for agent/secret_sources/base.py:reset_source_environment (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/secret_sources/base.py"
    PY_FN = "reset_source_environment"
    C_FN = "secret_reset_source_environment"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
