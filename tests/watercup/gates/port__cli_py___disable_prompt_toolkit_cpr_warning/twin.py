"""twin for cli.py:_disable_prompt_toolkit_cpr_warning (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cli.py"
    PY_FN = "_disable_prompt_toolkit_cpr_warning"
    C_FN = "_disable_prompt_toolkit_cpr_warning"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
