"""twin for tools/website_policy.py:invalidate_cache (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/website_policy.py"
    PY_FN = "invalidate_cache"
    C_FN = "cli_tools_website_policy__invalidate_cache"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
