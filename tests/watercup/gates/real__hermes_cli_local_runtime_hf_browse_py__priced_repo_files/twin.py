"""twin for hermes_cli/local_runtime/hf_browse.py:priced_repo_files (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/local_runtime/hf_browse.py"
    PY_FN = "priced_repo_files"
    C_FN = "priced_repo_files"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
