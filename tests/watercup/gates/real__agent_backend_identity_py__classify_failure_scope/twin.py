"""twin for agent/backend_identity.py:classify_failure_scope (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/backend_identity.py"
    PY_FN = "classify_failure_scope"
    C_FN = "agent_backend_identity_classify_failure_scope"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
