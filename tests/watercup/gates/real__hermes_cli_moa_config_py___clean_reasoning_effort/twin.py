"""twin for hermes_cli/moa_config.py:_clean_reasoning_effort (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/moa_config.py"
    PY_FN = "_clean_reasoning_effort"
    C_FN = "_clean_reasoning_effort"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
