"""twin for hermes_cli/secrets_cli.py:cmd_sync (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/secrets_cli.py"
    PY_FN = "cmd_sync"
    C_FN = "cmd_sync"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
