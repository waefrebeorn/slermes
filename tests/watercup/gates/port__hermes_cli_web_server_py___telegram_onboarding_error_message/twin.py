"""twin for hermes_cli/web_server.py:_telegram_onboarding_error_message (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/web_server.py"
    PY_FN = "_telegram_onboarding_error_message"
    C_FN = "web_telegram_onboarding_error_message"

    def norm(self, c, result):
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
