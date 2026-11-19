"""twin for hermes_cli/skills_config.py:_select_platform (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/skills_config.py"
    PY_FN = "_select_platform"
    C_FN = "_select_platform"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
