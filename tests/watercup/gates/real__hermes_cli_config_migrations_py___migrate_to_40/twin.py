"""twin for hermes_cli/config_migrations.py:_migrate_to_40 (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/config_migrations.py"
    PY_FN = "_migrate_to_40"
    C_FN = "_migrate_to_40"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
