"""twin for gateway/restart.py:resolve_systemd_timeout_stop_sec (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/restart.py"
    PY_FN = "resolve_systemd_timeout_stop_sec"
    C_FN = "resolve_systemd_timeout_stop_sec"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
