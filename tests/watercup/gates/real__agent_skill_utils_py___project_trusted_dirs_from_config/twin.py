"""twin for agent/skill_utils.py:_project_trusted_dirs_from_config (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/skill_utils.py"
    PY_FN = "_project_trusted_dirs_from_config"
    C_FN = "_project_trusted_dirs_from_config"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
