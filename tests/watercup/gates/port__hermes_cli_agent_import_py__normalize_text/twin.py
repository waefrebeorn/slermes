"""twin for hermes_cli/agent_import.py:normalize_text (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/agent_import.py"
    PY_FN = "normalize_text"
    C_FN = "ai_normalize_text"

    def norm(self, c, result):
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
