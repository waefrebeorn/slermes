"""twin for gateway/platforms/qqbot/adapter.py:enforces_own_access_policy (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/qqbot/adapter.py"
    PY_FN = "enforces_own_access_policy"
    C_FN = "qqa_enforces_own_access_policy"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
