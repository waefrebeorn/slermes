"""twin for gateway/relay/__init__.py:_relay_bot_ids_map (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/relay/__init__.py"
    PY_FN = "_relay_bot_ids_map"
    C_FN = "_relay_bot_ids_map"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
