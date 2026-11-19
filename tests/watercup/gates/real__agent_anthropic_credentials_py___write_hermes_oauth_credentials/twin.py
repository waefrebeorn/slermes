"""twin for agent/anthropic_credentials.py:_write_hermes_oauth_credentials (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/anthropic_credentials.py"
    PY_FN = "_write_hermes_oauth_credentials"
    C_FN = "_write_hermes_oauth_credentials"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
