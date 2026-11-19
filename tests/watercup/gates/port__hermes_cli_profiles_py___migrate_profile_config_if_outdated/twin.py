"""twin for hermes_cli/profiles.py:_migrate_profile_config_if_outdated (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/profiles.py"
    PY_FN = "_migrate_profile_config_if_outdated"
    C_FN = "profile_migrate_config_if_outdated"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
