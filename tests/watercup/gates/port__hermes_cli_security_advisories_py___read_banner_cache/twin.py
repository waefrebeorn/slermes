"""twin for hermes_cli/security_advisories.py:_read_banner_cache (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/security_advisories.py"
    PY_FN = "_read_banner_cache"
    C_FN = "_read_banner_cache"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
