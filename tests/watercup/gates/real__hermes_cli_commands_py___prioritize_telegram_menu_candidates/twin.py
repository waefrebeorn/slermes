"""twin for hermes_cli/commands.py:_prioritize_telegram_menu_candidates (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/commands.py"
    PY_FN = "_prioritize_telegram_menu_candidates"
    C_FN = "_prioritize_telegram_menu_candidates"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
