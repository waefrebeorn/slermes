"""twin for agent/secret_sources/bitwarden.py:_b64e (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/secret_sources/bitwarden.py"
    PY_FN = "_b64e"
    C_FN = "_b64e"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
