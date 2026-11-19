"""twin for hermes_cli/main.py:_guard_noninteractive_user_config (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/main.py"
    PY_FN = "_guard_noninteractive_user_config"
    C_FN = "_guard_noninteractive_user_config"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
