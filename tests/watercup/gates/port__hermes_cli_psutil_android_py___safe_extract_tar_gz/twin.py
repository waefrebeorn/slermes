"""twin for hermes_cli/psutil_android.py:_safe_extract_tar_gz (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/psutil_android.py"
    PY_FN = "_safe_extract_tar_gz"
    C_FN = "hermes_cli_psutil_android_u_safe_extract_tar_gz"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
