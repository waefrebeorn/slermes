"""twin for agent/anthropic_credentials.py:refresh_anthropic_oauth_pure (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/anthropic_credentials.py"
    PY_FN = "refresh_anthropic_oauth_pure"
    C_FN = "refresh_anthropic_oauth_pure"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
