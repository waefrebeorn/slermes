"""twin for gateway/relay/__init__.py:_post_policy (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/relay/__init__.py"
    PY_FN = "_post_policy"
    C_FN = "rly_post_policy"

    def norm(self, c, result):
        c.valstr(str(int(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
