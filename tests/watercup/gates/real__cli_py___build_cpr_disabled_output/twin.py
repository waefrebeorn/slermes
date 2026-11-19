"""twin for cli.py:_build_cpr_disabled_output (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cli.py"
    PY_FN = "_build_cpr_disabled_output"
    C_FN = "_build_cpr_disabled_output"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
