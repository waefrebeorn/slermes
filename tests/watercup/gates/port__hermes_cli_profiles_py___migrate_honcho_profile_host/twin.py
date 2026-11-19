"""twin for hermes_cli/profiles.py:_migrate_honcho_profile_host (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/profiles.py"
    PY_FN = "_migrate_honcho_profile_host"
    C_FN = "profile_migrate_honcho_profile_host"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
