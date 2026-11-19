"""twin for hermes_cli/process_identity.py:attach_self_to_kill_on_close_job (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/process_identity.py"
    PY_FN = "attach_self_to_kill_on_close_job"
    C_FN = "process_identity_attach_self_to_kill_on_close_job"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
