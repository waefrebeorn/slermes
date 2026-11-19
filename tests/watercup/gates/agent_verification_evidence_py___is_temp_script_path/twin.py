"""twin for agent/verification_evidence.py:_is_temp_script_path (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/verification_evidence.py"
    PY_FN = "_is_temp_script_path"
    C_FN = "verify_is_temp_script_path"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
