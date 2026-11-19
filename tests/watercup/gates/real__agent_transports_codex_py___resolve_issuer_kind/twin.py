"""twin for agent/transports/codex.py:_resolve_issuer_kind (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/transports/codex.py"
    PY_FN = "_resolve_issuer_kind"
    C_FN = "_resolve_issuer_kind"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
