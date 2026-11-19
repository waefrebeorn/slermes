"""twin for hermes_cli/web_server.py:_default_hermes_root_is_opt_data (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/web_server.py"
    PY_FN = "_default_hermes_root_is_opt_data"
    C_FN = "default_hermes_root_is_opt_data"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
