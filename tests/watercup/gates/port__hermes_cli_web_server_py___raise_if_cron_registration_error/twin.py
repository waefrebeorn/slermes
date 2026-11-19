"""twin for hermes_cli/web_server.py:_raise_if_cron_registration_error (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/web_server.py"
    PY_FN = "_raise_if_cron_registration_error"
    C_FN = "_raise_if_cron_registration_error"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
