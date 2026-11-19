"""twin for hermes_cli/config.py:_chown_to_hermes_uid (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/config.py"
    PY_FN = "_chown_to_hermes_uid"
    C_FN = "config_py_chown_to_hermes_uid"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
