"""twin for gateway/hosted_room_replicas.py:ingest_page (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/hosted_room_replicas.py"
    PY_FN = "ingest_page"
    C_FN = "ingest_page"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
