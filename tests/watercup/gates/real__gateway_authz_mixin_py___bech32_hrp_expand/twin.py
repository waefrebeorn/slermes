"""twin for gateway/authz_mixin.py:_bech32_hrp_expand (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/authz_mixin.py"
    PY_FN = "_bech32_hrp_expand"
    C_FN = "_bech32_hrp_expand"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
