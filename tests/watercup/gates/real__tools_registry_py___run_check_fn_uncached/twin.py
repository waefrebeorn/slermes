"""twin for tools/registry.py:_run_check_fn_uncached (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/registry.py"
    PY_FN = "_run_check_fn_uncached"
    C_FN = "_run_check_fn_uncached"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
