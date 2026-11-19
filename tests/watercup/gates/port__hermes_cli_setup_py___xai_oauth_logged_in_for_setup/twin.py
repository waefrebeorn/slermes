"""twin for hermes_cli/setup.py:_xai_oauth_logged_in_for_setup (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/setup.py"
    PY_FN = "_xai_oauth_logged_in_for_setup"
    C_FN = "stp_xai_oauth_logged_in_for_setup"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
