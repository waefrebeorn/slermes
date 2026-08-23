"""twin for models_normalize_opencode_base_url.

Imports LIVE hermes_cli/models.py::normalize_opencode_base_url and traces the
SAME rung ladder as the C harness
(models_py_helpers.c::models_normalize_opencode_base_url).
Inputs are "<provider_id>|<api_mode>|<base_url>".
"""
import os
import sys
import importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

spec = ilu.spec_from_file_location(
    "wc_live_models", os.path.join(DEV, "hermes_cli", "models.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_live_models"] = mod
spec.loader.exec_module(mod)

from watercup_py import Cup


def run(inputs):
    cups = []
    for raw in inputs:
        c = Cup("normalize_opencode_base_url")
        provider_id, api_mode, base_url = raw.split("|", 2)
        provider = None if provider_id in ("", "null") else provider_id
        url_arg = None if base_url == "null" else base_url

        url = str(url_arg or "").strip().rstrip("/")
        c.valstr(url)
        result = url
        if not url:
            c.branch(True)
        else:
            c.branch(False)
            family = mod.opencode_provider_family(provider)
            # C port recognizes only opencode-zen/opencode-go families.
            if family not in ("opencode-zen", "opencode-go"):
                c.branch(True)
            else:
                c.branch(False)
                if api_mode == "anthropic_messages":
                    c.branch(True)
                    import re as _re
                    result = _re.sub(r"/v1$", "", url)
                else:
                    c.branch(False)
                    if url.endswith("/v1"):
                        c.branch(True)
                        result = url
                    else:
                        c.branch(False)
                        try:
                            import urllib.parse as _up
                            host = _up.urlparse(url).netloc.lower()
                        except Exception:
                            host = ""
                        c.valstr(host)
                        if host == "opencode.ai" or host.endswith(".opencode.ai"):
                            c.branch(True)
                            result = url + "/v1"
                        else:
                            c.branch(False)
        c.valstr(result)
        cups.append(c.finalize())
    return cups
