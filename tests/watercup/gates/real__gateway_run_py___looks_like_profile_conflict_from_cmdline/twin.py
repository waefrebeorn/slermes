"""twin for gateway/run.py:_looks_like_profile_conflict_from_cmdline (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/run.py"
    PY_FN = "_looks_like_profile_conflict_from_cmdline"
    C_FN = "gw_looks_like_profile_conflict_from_cmdline"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
