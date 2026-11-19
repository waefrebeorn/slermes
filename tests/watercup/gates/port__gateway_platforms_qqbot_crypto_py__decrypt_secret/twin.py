"""twin for gateway/platforms/qqbot/crypto.py:decrypt_secret (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/qqbot/crypto.py"
    PY_FN = "decrypt_secret"
    C_FN = "decrypt_secret"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
