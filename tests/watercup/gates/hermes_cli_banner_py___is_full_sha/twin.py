"""twin for hermes_cli/banner.py:_is_full_sha (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/banner.py"
    PY_FN = "_is_full_sha"
    C_FN = "banner_is_full_sha"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
