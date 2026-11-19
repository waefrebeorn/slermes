"""twin for tools/bot_relay.py:cleanup_bot_relay_artifacts (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/bot_relay.py"
    PY_FN = "cleanup_bot_relay_artifacts"
    C_FN = "cleanup_bot_relay_artifacts"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
