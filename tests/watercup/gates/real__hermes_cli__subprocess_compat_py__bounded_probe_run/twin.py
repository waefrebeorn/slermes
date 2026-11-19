"""twin for hermes_cli/_subprocess_compat.py:bounded_probe_run (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/_subprocess_compat.py"
    PY_FN = "bounded_probe_run"
    C_FN = "bounded_probe_run"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
