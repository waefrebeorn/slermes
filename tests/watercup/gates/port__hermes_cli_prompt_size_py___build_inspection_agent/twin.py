"""twin for hermes_cli/prompt_size.py:_build_inspection_agent (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/prompt_size.py"
    PY_FN = "_build_inspection_agent"
    C_FN = "_build_inspection_agent"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
