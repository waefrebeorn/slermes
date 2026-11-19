"""twin for gateway/pairing.py:_read_allowlist_env (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/pairing.py"
    PY_FN = "_read_allowlist_env"
    C_FN = "_read_allowlist_env"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
