"""twin for hermes_cli/web_server.py:_voice_list_error_logged_once (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/web_server.py"
    PY_FN = "_voice_list_error_logged_once"
    C_FN = "web_voice_list_error_logged_once"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
