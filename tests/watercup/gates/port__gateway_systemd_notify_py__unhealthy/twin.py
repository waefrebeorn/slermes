"""twin for gateway/systemd_notify.py:unhealthy (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/systemd_notify.py"
    PY_FN = "unhealthy"
    C_FN = "sd_n_unhealthy_state"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
