"""twin for agent/credential_persistence.py:_is_secret_payload_key (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/credential_persistence.py"
    PY_FN = "_is_secret_payload_key"
    C_FN = "is_secret_payload_key"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
