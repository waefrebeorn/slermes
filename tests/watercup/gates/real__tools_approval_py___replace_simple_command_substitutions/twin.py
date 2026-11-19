"""twin for tools/approval.py:_replace_simple_command_substitutions (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/approval.py"
    PY_FN = "_replace_simple_command_substitutions"
    C_FN = "_replace_simple_command_substitutions"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
