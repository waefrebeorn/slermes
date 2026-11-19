"""twin for agent/redact.py:_key_has_secret_keyword (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/redact.py"
    PY_FN = "_key_has_secret_keyword"
    C_FN = "agent_redact_key_has_secret_keyword"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
