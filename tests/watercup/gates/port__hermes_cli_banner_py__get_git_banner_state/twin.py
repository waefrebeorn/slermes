"""twin for hermes_cli/banner.py:get_git_banner_state (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/banner.py"
    PY_FN = "get_git_banner_state"
    C_FN = "get_git_banner_state"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
