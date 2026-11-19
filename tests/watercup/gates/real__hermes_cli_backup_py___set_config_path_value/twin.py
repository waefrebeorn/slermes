"""twin for hermes_cli/backup.py:_set_config_path_value (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/backup.py"
    PY_FN = "_set_config_path_value"
    C_FN = "_set_config_path_value"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
