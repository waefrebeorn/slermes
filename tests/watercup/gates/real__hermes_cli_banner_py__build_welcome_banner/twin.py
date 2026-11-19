"""twin for hermes_cli/banner.py:build_welcome_banner (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/banner.py"
    PY_FN = "build_welcome_banner"
    C_FN = "build_welcome_banner"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
