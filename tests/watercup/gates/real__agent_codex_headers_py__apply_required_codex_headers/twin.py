"""twin for agent/codex_headers.py:apply_required_codex_headers (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/codex_headers.py"
    PY_FN = "apply_required_codex_headers"
    C_FN = "apply_required_codex_headers"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
