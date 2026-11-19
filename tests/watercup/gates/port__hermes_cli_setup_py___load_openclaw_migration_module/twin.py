"""twin for hermes_cli/setup.py:_load_openclaw_migration_module (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/setup.py"
    PY_FN = "_load_openclaw_migration_module"
    C_FN = "_load_openclaw_migration_module"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
