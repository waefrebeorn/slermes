"""twin for gateway/run.py:_get_guild_id (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/run.py"
    PY_FN = "_get_guild_id"
    C_FN = "_get_guild_id"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
