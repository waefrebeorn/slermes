"""twin for gateway/relay/auth.py:verify_delivery_signature (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/relay/auth.py"
    PY_FN = "verify_delivery_signature"
    C_FN = "rlauth_verify_delivery_signature"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
