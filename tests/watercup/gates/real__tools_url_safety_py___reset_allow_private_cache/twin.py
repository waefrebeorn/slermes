"""twin for tools/url_safety.py:_reset_allow_private_cache (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/url_safety.py"
    PY_FN = "_reset_allow_private_cache"
    C_FN = "cli_tools_url_safety__reset_allow_private_cache"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
