"""twin for gateway/run.py:_stamp_hygiene_compression_provenance (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/run.py"
    PY_FN = "_stamp_hygiene_compression_provenance"
    C_FN = "gw_stamp_hygiene_compression_provenance"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
