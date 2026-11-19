"""twin for hermes_cli/banner.py:_banner_snapshot_path (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/banner.py"
    PY_FN = "_banner_snapshot_path"
    C_FN = "_banner_snapshot_path"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
