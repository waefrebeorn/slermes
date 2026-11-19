"""twin for hermes_cli/nous_account.py:_paid_service_access_from_payload (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/nous_account.py"
    PY_FN = "_paid_service_access_from_payload"
    C_FN = "nous_u_paid_service_access_from_payload"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
