"""twin for (contract — no LIVE python twin; self-contained parity gate):Cache.get (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "(contract — no LIVE python twin; self-contained parity gate)"
    PY_FN = "Cache.get"
    C_FN = "Cache.get"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
