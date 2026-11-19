"""twin for gateway/slash_access.py:SlashAccessPolicy.can_run (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/slash_access.py"
    PY_FN = "SlashAccessPolicy.can_run"
    C_FN = "SlashAccessPolicy.can_run"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
