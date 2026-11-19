"""twin for agent/anthropic_adapter.py:_is_third_party_anthropic_endpoint (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/anthropic_adapter.py"
    PY_FN = "_is_third_party_anthropic_endpoint"
    C_FN = "antr_is_third_party_anthropic_endpoint"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
