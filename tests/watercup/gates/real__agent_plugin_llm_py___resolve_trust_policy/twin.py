"""twin for agent/plugin_llm.py:_resolve_trust_policy (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/plugin_llm.py"
    PY_FN = "_resolve_trust_policy"
    C_FN = "_resolve_trust_policy"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
