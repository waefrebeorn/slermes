"""twin for gateway/rich_sent_store.py:_store_path (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/rich_sent_store.py"
    PY_FN = "_store_path"
    C_FN = "_store_path"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
