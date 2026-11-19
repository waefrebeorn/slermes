"""twin for gateway/authz_mixin.py:_adapter_authorization_is_upstream (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/authz_mixin.py"
    PY_FN = "_adapter_authorization_is_upstream"
    C_FN = "authz_adapter_authorization_is_upstream"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
