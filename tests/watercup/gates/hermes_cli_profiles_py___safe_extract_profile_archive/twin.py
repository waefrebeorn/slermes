"""twin for hermes_cli/profiles.py:_safe_extract_profile_archive (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/profiles.py"
    PY_FN = "_safe_extract_profile_archive"
    C_FN = "profile_safe_extract_archive"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
