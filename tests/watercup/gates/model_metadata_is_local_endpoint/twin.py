"""twin for model_metadata_is_local_endpoint.

Traces the LIVE agent/model_metadata.py::is_local_endpoint decision structure
with the SAME rung ladder as the C harness (see harness.c), using the LIVE
module's own _normalize_base_url / urlparse / ipaddress machinery for values.
The final result is cross-checked against a direct call to the LIVE function.
"""
import os
import sys
import importlib.util as ilu
from urllib.parse import urlparse
from ipaddress import ip_address

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_model_metadata",
    os.path.join(DEV, "agent", "model_metadata.py"))
_mm = ilu.module_from_spec(_spec)
sys.modules["wc_live_model_metadata"] = _mm
if _spec.loader is not None:
    _spec.loader.exec_module(_mm)

LOCAL_HOSTS = ("localhost", "127.0.0.1", "::1", "0.0.0.0", "127.0.1.1")
SUFFIXES = ("host.docker.internal", "host.podman.internal", "host.lima.internal",
            ".host.docker.internal", ".host.podman.internal", ".host.lima.internal")


def trace(base_url):
    c = Cup("is_local_endpoint")
    normalized = _mm._normalize_base_url(base_url)

    c.branch(not normalized)                                       # B1
    if not normalized:
        result = False
        hit = True
    else:
        url = normalized if "://" in normalized else "http://" + normalized
        host = urlparse(url).hostname or ""

        c.valstr(host)
        hit = host.lower() in LOCAL_HOSTS
        c.branch(hit)                                              # B2
        if hit:
            result = True
        else:
            hlow = host.lower()
            cs = any(hlow.endswith(sfx) for sfx in SUFFIXES)
            c.branch(cs)                                           # B3
            if cs:
                result = True
                hit = True
            else:
                try:
                    addr = ip_address(host)
                    v4 = addr.version == 4
                except ValueError:
                    addr, v4 = None, False
                c.branch(v4)                                       # B4
                if not v4:
                    c.branch(addr is not None or _is_v6(host))     # B5
                    result = False
                    hit = True
                else:
                    b = addr.packed
                    ip = (b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3]
                    result = False
                    for mask, val in ((0xFF000000, 0x7F000000),
                                      (0xFF000000, 0x0A000000),
                                      (0xFFF00000, 0xAC100000),
                                      (0xFFFF0000, 0xC0A80000),
                                      (0xFFFF0000, 0xA9FE0000),
                                      (0xFFC00000, 0x64400000)):
                        r = (ip & mask) == val
                        c.branch(r)
                        if r:
                            result = True
                            break
                    if not result:
                        r = ((ip >> 24) & 0xFF) == 0
                        c.branch(r)                                # 0/8
                        if r:
                            result = True
                    if not result:
                        c.branch(False)                            # B5 (v6 parse)
                    hit = True
    if hit:
        c.valstr("true" if result else "false")
    live = bool(_mm.is_local_endpoint(base_url))
    assert live == bool(result), (
        "twin decomposition diverged from LIVE is_local_endpoint(%r)" % base_url)
    return c.finalize()


def _is_v6(host):
    try:
        return ip_address(host).version == 6
    except ValueError:
        return False


def run(inputs):
    return [trace(raw) for raw in inputs]
