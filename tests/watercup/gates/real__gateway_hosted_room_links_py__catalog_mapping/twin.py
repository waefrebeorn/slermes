"""twin for gateway/hosted_room_links.py:catalog_mapping (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/hosted_room_links.py"
    PY_FN = "catalog_mapping"
    C_FN = "catalog_mapping"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
