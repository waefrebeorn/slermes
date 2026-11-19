"""twin for hermes_cli/_startup_fast.py:container_mode_may_be_active (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/_startup_fast.py"
    PY_FN = "container_mode_may_be_active"
    C_FN = "sf_container_mode_may_be_active"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
