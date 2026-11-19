"""twin for hermes_cli/copilot_auth.py:_exchange_copilot_token_locked (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/copilot_auth.py"
    PY_FN = "_exchange_copilot_token_locked"
    C_FN = "_exchange_copilot_token_locked"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
