"""twin for hermes_cli/nous_account.py:format_nous_portal_entitlement_message (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/nous_account.py"
    PY_FN = "format_nous_portal_entitlement_message"
    C_FN = "nous_format_nous_portal_entitlement_message"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
