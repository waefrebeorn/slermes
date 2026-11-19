"""twin for hermes_cli/setup.py:_is_valid_telegram_bot_token (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/setup.py"
    PY_FN = "_is_valid_telegram_bot_token"
    C_FN = "stp_is_valid_telegram_bot_token"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
