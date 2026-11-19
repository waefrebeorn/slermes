"""twin for agent/error_classifier.py:_is_openrouter_upstream_error (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/error_classifier.py"
    PY_FN = "_is_openrouter_upstream_error"
    C_FN = "cli_agent_error_classifier__is_openrouter_upstream_error"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
