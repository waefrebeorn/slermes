"""twin for agent/skill_utils.py:parse_config_string_list (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/skill_utils.py"
    PY_FN = "parse_config_string_list"
    C_FN = "parse_config_string_list"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
