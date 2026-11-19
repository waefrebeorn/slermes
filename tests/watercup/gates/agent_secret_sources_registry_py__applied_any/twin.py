"""twin for agent/secret_sources/registry.py:applied_any (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/secret_sources/registry.py"
    PY_FN = "applied_any"
    C_FN = "ssr_applied_any"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
