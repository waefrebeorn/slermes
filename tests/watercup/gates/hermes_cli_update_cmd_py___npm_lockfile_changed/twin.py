"""twin for hermes_cli/update_cmd.py:_npm_lockfile_changed (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/update_cmd.py"
    PY_FN = "_npm_lockfile_changed"
    C_FN = "uc_npm_lockfile_changed"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
