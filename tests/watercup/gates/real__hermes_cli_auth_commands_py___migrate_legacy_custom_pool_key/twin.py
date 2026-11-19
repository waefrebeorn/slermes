"""twin for hermes_cli/auth_commands.py:_migrate_legacy_custom_pool_key (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/auth_commands.py"
    PY_FN = "_migrate_legacy_custom_pool_key"
    C_FN = "_migrate_legacy_custom_pool_key"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
