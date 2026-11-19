"""twin for hermes_cli/update_cmd.py:_is_zip_staging_artifact_status_line (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/update_cmd.py"
    PY_FN = "_is_zip_staging_artifact_status_line"
    C_FN = "upd_is_zip_staging_artifact_status_line"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
