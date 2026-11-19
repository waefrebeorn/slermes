"""twin for agent/ssl_verify.py:_coerce_insecure (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/ssl_verify.py"
    PY_FN = "_coerce_insecure"
    C_FN = "agent_ssl_verify_coerce_insecure"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
