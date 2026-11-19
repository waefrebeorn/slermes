"""twin for gateway/rich_sent_store.py:record (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/rich_sent_store.py"
    PY_FN = "record"
    C_FN = "rich_sent_record"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
