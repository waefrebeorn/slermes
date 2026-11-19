"""twin for hermes_cli/build_info.py:get_code_identity (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/build_info.py"
    PY_FN = "get_code_identity"
    C_FN = "get_code_identity"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
