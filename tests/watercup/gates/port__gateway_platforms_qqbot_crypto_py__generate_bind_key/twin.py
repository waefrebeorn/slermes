"""twin for gateway/platforms/qqbot/crypto.py:generate_bind_key (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/qqbot/crypto.py"
    PY_FN = "generate_bind_key"
    C_FN = "generate_bind_key"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
