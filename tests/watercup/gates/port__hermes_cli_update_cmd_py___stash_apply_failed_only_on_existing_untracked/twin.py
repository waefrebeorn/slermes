"""twin for hermes_cli/update_cmd.py:_stash_apply_failed_only_on_existing_untracked (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/update_cmd.py"
    PY_FN = "_stash_apply_failed_only_on_existing_untracked"
    C_FN = "uc_stash_apply_failed_only_on_existing_untracked"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
