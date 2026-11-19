"""twin for gateway/shutdown_forensics.py:parse_systemd_duration_to_us (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/shutdown_forensics.py"
    PY_FN = "parse_systemd_duration_to_us"
    C_FN = "cli_gateway_shutdown_forensics__parse_systemd_duration_to_us"

    def norm(self, c, result):
        c.valstr(str(int(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
