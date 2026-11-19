"""twin for hermes_cli/web_server.py:_valid_parent_start_marker (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/web_server.py"
    PY_FN = "_valid_parent_start_marker"
    C_FN = "ws_valid_parent_start_marker"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
