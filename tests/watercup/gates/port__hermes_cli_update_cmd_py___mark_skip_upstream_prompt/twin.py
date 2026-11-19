"""twin for hermes_cli/update_cmd.py:_mark_skip_upstream_prompt (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/update_cmd.py"
    PY_FN = "_mark_skip_upstream_prompt"
    C_FN = "uc_mark_skip_upstream_prompt"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
