"""twin for hermes_cli/local_runtime/supervisor.py:_url (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/local_runtime/supervisor.py"
    PY_FN = "_url"
    C_FN = "_url"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
