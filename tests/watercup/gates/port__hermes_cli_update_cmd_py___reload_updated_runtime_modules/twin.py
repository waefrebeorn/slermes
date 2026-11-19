"""twin for hermes_cli/update_cmd.py:_reload_updated_runtime_modules (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/update_cmd.py"
    PY_FN = "_reload_updated_runtime_modules"
    C_FN = "uc_reload_updated_runtime_modules"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
