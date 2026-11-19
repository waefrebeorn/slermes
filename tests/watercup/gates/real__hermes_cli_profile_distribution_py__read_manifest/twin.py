"""twin for hermes_cli/profile_distribution.py:read_manifest (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/profile_distribution.py"
    PY_FN = "read_manifest"
    C_FN = "read_manifest"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
