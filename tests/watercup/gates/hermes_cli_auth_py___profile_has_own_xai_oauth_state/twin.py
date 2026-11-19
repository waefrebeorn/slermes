"""twin for hermes_cli/auth.py:_profile_has_own_xai_oauth_state (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/auth.py"
    PY_FN = "_profile_has_own_xai_oauth_state"
    C_FN = "ath_profile_has_own_xai_oauth_state"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
