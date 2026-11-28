"""twin for agent/redact.py:_canonical_url_param_name."""
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/redact.py"
    PY_FN = "_canonical_url_param_name"
    C_FN = "agent_redact_u_canonical_url_param_name"
    def norm(self, c, result):
        c.valstr("" if result is None else str(result))
_t = _Twin(); trace = _t.trace; run = _t.run
