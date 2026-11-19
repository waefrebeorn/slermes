"""twin for hermes_cli/build_info.py:_resolve_git_head_sha (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/build_info.py"
    PY_FN = "_resolve_git_head_sha"
    C_FN = "_resolve_git_head_sha"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
