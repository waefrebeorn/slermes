"""twin for tools/bot_relay.py:write_remote_roster (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/bot_relay.py"
    PY_FN = "write_remote_roster"
    C_FN = "write_remote_roster"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
