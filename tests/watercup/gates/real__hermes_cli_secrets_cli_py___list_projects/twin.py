"""twin for hermes_cli/secrets_cli.py:_list_projects (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/secrets_cli.py"
    PY_FN = "_list_projects"
    C_FN = "_list_projects"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
