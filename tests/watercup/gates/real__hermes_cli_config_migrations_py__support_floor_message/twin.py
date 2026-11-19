"""twin for hermes_cli/config_migrations.py:support_floor_message (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/config_migrations.py"
    PY_FN = "support_floor_message"
    C_FN = "support_floor_message"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
