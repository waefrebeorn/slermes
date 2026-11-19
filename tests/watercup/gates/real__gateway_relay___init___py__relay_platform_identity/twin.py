"""twin for gateway/relay/__init__.py:relay_platform_identity (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/relay/__init__.py"
    PY_FN = "relay_platform_identity"
    C_FN = "relay_platform_identity"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
