"""twin: tools/environments/file_sync.py:_sync_back_max_bytes.

The real function is called; only hermes_cli.config.load_config is swapped so the
fixture stands in for config.yaml. Calling int(raw) here instead would re-test my
own reimplementation rather than the live code.
"""
import json as _j
import hermes_cli.config as _cfg
import tools.environments.file_sync as _fs
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "tools/environments/file_sync.py"
    PY_FN = "_sync_back_max_bytes"
    C_FN = "cli_tools_environments_file_sync_sync_back_max_bytes_resolve"
    def call(self, inp, inputs, idx=0):
        raw = _j.loads(inp)
        _cfg.load_config = lambda *a, **k: {"terminal": {"sync_back_max_bytes": raw}}
        return _fs._sync_back_max_bytes()
    def norm(self, c, result):
        c.valstr(str(result))
_t=_Twin(); trace=_t.trace; run=_t.run
