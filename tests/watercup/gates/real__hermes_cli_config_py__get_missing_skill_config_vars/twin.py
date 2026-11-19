"""twin for hermes_cli/config.py:get_missing_skill_config_vars (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/config.py"
    PY_FN = "get_missing_skill_config_vars"
    C_FN = "get_missing_skill_config_vars"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
