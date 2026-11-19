"""twin for agent/auxiliary_client.py:_is_anthropic_compat_endpoint (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/auxiliary_client.py"
    PY_FN = "_is_anthropic_compat_endpoint"
    C_FN = "auxc_is_anthropic_compat_endpoint"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
