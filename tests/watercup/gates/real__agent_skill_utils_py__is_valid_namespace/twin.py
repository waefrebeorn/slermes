"""twin: is_valid_namespace(candidate) -> bool."""
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/skill_utils.py"; PY_FN = "is_valid_namespace"
    C_FN = "agent_skill_utils_u_is_valid_namespace"
    def norm(self, c, result):
        c.valstr("1" if result else "0")
_t = _Twin(); trace = _t.trace; run = _t.run
