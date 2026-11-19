"""twin for hermes_cli/telegram_managed_bot.py:_parse_owner_user_id (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/telegram_managed_bot.py"
    PY_FN = "_parse_owner_user_id"
    C_FN = "hermes_cli_telegram_managed_bo_u_parse_owner_user_id"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
