"""twin for agent/auxiliary_client.py:_is_unsupported_parameter_error (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/auxiliary_client.py"
    PY_FN = "_is_unsupported_parameter_error"
    C_FN = "auxc_is_unsupported_parameter_error"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
