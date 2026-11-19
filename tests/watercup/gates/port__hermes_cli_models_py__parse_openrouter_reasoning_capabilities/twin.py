"""twin for hermes_cli/models.py:parse_openrouter_reasoning_capabilities (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/models.py"
    PY_FN = "parse_openrouter_reasoning_capabilities"
    C_FN = "parse_openrouter_reasoning_capabilities"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
