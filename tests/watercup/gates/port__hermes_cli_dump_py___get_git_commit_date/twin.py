"""twin for hermes_cli/dump.py:_get_git_commit_date (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/dump.py"
    PY_FN = "_get_git_commit_date"
    C_FN = "_get_git_commit_date"

    def norm(self, c, result):
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
