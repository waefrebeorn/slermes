"""twin for agent/secret_sources/base.py:scrub_ansi (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/secret_sources/base.py"
    PY_FN = "scrub_ansi"
    C_FN = "scrub_ansi"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
