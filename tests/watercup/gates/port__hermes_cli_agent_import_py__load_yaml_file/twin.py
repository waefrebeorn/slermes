"""twin for hermes_cli/agent_import.py:load_yaml_file (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/agent_import.py"
    PY_FN = "load_yaml_file"
    C_FN = "load_yaml_file"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
