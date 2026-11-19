"""twin for hermes_cli/gateway_enroll.py:_default_gateway_id (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/gateway_enroll.py"
    PY_FN = "_default_gateway_id"
    C_FN = "hermes_cli_gateway_enroll_u_default_gateway_id"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
