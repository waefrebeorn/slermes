"""twin for agent/shell_hooks.py:_command_script_path (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/shell_hooks.py"
    PY_FN = "_command_script_path"
    C_FN = "_command_script_path"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
