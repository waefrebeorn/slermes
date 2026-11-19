"""twin for hermes_cli/dashboard_procs.py:_profile_key_for_respawn (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/dashboard_procs.py"
    PY_FN = "_profile_key_for_respawn"
    C_FN = "dashboard_procs_profile_key_for_respawn"

    def norm(self, c, result):
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
