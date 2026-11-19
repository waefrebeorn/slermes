"""twin for hermes_cli/heartbeat.py:migrate_heartbeat_to_session (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/heartbeat.py"
    PY_FN = "migrate_heartbeat_to_session"
    C_FN = "hb_migrate_heartbeat_to_session"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
