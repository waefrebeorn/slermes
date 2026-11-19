"""twin for tools/environments/local.py:_additional_profile_scoped_passthrough_names (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/environments/local.py"
    PY_FN = "_additional_profile_scoped_passthrough_names"
    C_FN = "_additional_profile_scoped_passthrough_names"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
