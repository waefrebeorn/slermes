"""twin for hermes_cli/browser_connect.py:cleanup_real_profile_snapshots (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/browser_connect.py"
    PY_FN = "cleanup_real_profile_snapshots"
    C_FN = "cleanup_real_profile_snapshots"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
