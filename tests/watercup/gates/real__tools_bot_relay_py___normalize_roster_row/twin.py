"""twin for tools/bot_relay.py:_normalize_roster_row (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/bot_relay.py"
    PY_FN = "_normalize_roster_row"
    C_FN = "_normalize_roster_row"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
