"""twin for agent/ssl_verify.py:_context_for_ca_bundle (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/ssl_verify.py"
    PY_FN = "_context_for_ca_bundle"
    C_FN = "_context_for_ca_bundle"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
