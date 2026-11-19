"""twin for hermes_cli/install_identity.py:_fsync_directory (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/install_identity.py"
    PY_FN = "_fsync_directory"
    C_FN = "_fsync_directory"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
