#!/usr/bin/env python3
"""Differential oracle for hermes_platform/host/darwin_pci.py (gate w25_darwinpci_ff026b).

IOKit is the seam.  ``_data_int`` is the live function, called with a fake
CoreFoundation object that hands it the bytes a property carried -- the same
``ctypes.string_at`` read, the same type check, the same ``int.from_bytes``.
The registry walk itself (``display_vendor_ids``) cannot run off a Mac, so the
oracle restates its two-line filter against the live ``_DISPLAY_CLASS`` and
``_MAX_DEVICES`` and reuses the live ``_data_int`` for every property read.
"""
import ctypes
import json
import os
import sys
import tempfile
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

if not os.environ.get("USERPROFILE"):
    os.environ["USERPROFILE"] = str(Path(tempfile.gettempdir()))

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w25_darwinpci_ff026b"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from hermes_platform.host import darwin_pci as live  # noqa: E402

WANTED = {"data_int", "format_vendor", "display_vendor_ids", "constants"}


class _FakeCF:
    """The CoreFoundation surface ``_data_int`` touches, over fixture bytes."""

    def __init__(self):
        self._values = {}
        self._next = 1000

    def add(self, data):
        self._next += 1
        self._values[self._next] = data
        return self._next

    def CFDataGetTypeID(self):
        return 1

    def CFGetTypeID(self, ref):
        return 1 if isinstance(self._values.get(ref), bytes) else 2

    def CFDataGetLength(self, ref):
        return len(self._values[ref])

    def CFDataGetBytePtr(self, ref):
        d = self._values[ref]
        return (ctypes.c_char * len(d)).from_buffer_copy(d)

    def CFRelease(self, ref):
        return None


def _bytes_of(hexstr):
    """A fixture property value: hex text, or None for missing / not data."""
    if hexstr is None:
        return None
    if not isinstance(hexstr, str) or len(hexstr) % 2:
        return None
    try:
        return bytes.fromhex(hexstr)
    except ValueError:
        return None


def _read(cf, hexstr):
    data = _bytes_of(hexstr)
    if data is None:
        return live._data_int(cf, None)
    return live._data_int(cf, cf.add(data))


def data_int(req):
    return _read(_FakeCF(), req.get("hex"))


def format_vendor(req):
    value = req.get("value")
    return "" if value is None else f"{int(value) & 0xFFFF:04x}"


def display_vendor_ids(req):
    if req.get("readable") is False:
        return None
    devices = req.get("devices")
    if not isinstance(devices, list):
        return None
    cf = _FakeCF()
    vendors = []
    for entry in devices[: live._MAX_DEVICES]:
        if not isinstance(entry, dict):
            continue
        class_code = _read(cf, entry.get("class_code"))
        vendor = _read(cf, entry.get("vendor_id"))
        if class_code is not None and class_code >> 16 == live._DISPLAY_CLASS:
            vendors.append("" if vendor is None else f"{vendor & 0xFFFF:04x}")
    return vendors


def constants(_req):
    return {
        "display_class": live._DISPLAY_CLASS,
        "max_devices": live._MAX_DEVICES,
        "utf8_encoding": live._UTF8,
        "service_class": "IOPCIDevice",
        "class_code_key": "class-code",
        "vendor_id_key": "vendor-id",
    }


def expected(r):
    f = r.get("func")
    return {
        "data_int": data_int,
        "format_vendor": format_vendor,
        "display_vendor_ids": display_vendor_ids,
        "constants": constants,
    }[f](r)


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in WANTED:
            continue
        cases += 1
        try:
            py = expected(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r"
                  % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
