"""twin for tools/environments/local.py:_build_hermes_repo_root_aliases (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/environments/local.py"
    PY_FN = "_build_hermes_repo_root_aliases"
    C_FN = "_build_hermes_repo_root_aliases"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
