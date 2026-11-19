"""twin for hermes_cli/web_server.py:_maybe_auto_archive_for_profile (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/web_server.py"
    PY_FN = "_maybe_auto_archive_for_profile"
    C_FN = "_maybe_auto_archive_for_profile"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
