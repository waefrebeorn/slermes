"""twin for hermes_cli/gateway.py:_hermes_home_from_systemd_unit_file (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/gateway.py"
    PY_FN = "_hermes_home_from_systemd_unit_file"
    C_FN = "cgw_u_hermes_home_from_systemd_unit_file"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
