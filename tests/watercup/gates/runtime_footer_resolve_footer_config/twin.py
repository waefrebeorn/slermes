"""twin for gateway/runtime_footer.py:resolve_footer_config (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/runtime_footer.py"
    PY_FN = "resolve_footer_config"
    C_FN = "resolve_footer_config"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
