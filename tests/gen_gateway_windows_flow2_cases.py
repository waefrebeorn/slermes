#!/usr/bin/env python3
"""Generate tests/fixtures/gateway_windows_flow2.jsonl — scripted-seam FLOW
cases for the gateway_windows_flow2 port (hermes_cli/gateway_windows.py
unclaimed remainder: start-attestation lifecycle, scheduled-task XML drift,
launcher rendering/atomic-write, small report/path helpers).

Every case: {"func": <harness name>, "in": {... inputs ..., "seams": {name: script}}}
A seam script is a single result, or a JSON array = one result PER CALL (last
repeats). {"__raise__": cls, "msg": m} raises that exception class in the live
drive and is delivered as a raise descriptor to the C universal seam. A MISSING
seam key fail-closes (RuntimeError) on BOTH sides.

Domains (documented in the port header):
- paths are POSIX strings; pids/create_times are integral JSON numbers (no
  integral-float literals: libjson has no int/float identity).
- ISO timestamps: extended YYYY-MM-DD[T ]HH:MM[:SS[.frac]][Z|+HH[:MM]] forms.
- XML: elements/attributes/entities/comments/CDATA/PI/DOCTYPE/BOM/namespaces;
  no DTD internal-subset entities in text, ASCII tags.
Run: python tests/gen_gateway_windows_flow2_cases.py
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "gateway_windows_flow2.jsonl")

CASES = []


def add(func, inp, seams):
    inp = dict(inp)
    inp["seams"] = seams
    CASES.append({"func": func, "in": inp})


def raise_spec(cls, msg=""):
    return {"__raise__": cls, "msg": msg}


MARKER = "/h/state/gateway.start-attestation.json"
NOW = 1795005296          # 2026-11-18T12:34:56+00:00 (probe-verified epoch)
TS_FRESH = "2026-11-18T12:34:56+00:00"   # epoch NOW
TS_NAIVE = "2026-11-18T12:34:56"         # naive -> UTC -> epoch NOW


def payload(pids=(123, 456), via="direct spawn", ts=TS_FRESH, gen="gen0001",
            create_times=None):
    p = {"pids": list(pids), "via": via, "ts": ts, "generation": gen}
    if create_times is not None:
        p["create_times"] = create_times
    return p


def marker_seams(data=None, **over):
    """Scripts for flows reading the marker through the ported read helper."""
    s = {
        "start_attestation_path": MARKER,
        "path_read_text": json.dumps(data) if data is not None else None,
        "path_unlink": None,
    }
    s.update(over)
    return s


# ── _attested_pids_from ──
add("attested_pids_from", {"data": payload()}, {})
add("attested_pids_from", {"data": payload(pids=[])}, {})
add("attested_pids_from", {"data": payload(pids=[1])}, {})
add("attested_pids_from", {"data": None}, {})
add("attested_pids_from", {"data": [1, 2]}, {})
add("attested_pids_from", {"data": "x"}, {})
add("attested_pids_from", {"data": {"pids": None}}, {})
add("attested_pids_from", {"data": {"pids": "123"}}, {})
add("attested_pids_from", {"data": {"pids": [0, 5]}}, {})
add("attested_pids_from", {"data": {"pids": [-3]}}, {})
add("attested_pids_from", {"data": {"pids": [True, 5]}}, {})
add("attested_pids_from", {"data": {"pids": [5.5]}}, {})
add("attested_pids_from", {"data": {"pids": ["5"]}}, {})
add("attested_pids_from", {"data": {"pids": [5, None]}}, {})
add("attested_pids_from", {"data": {"pids": [7, 8, 9]}}, {})

# ── _attestation_generation ──
add("attestation_generation", {"data": payload(gen="abc")}, {})
add("attestation_generation", {"data": payload(gen=7)}, {})
add("attestation_generation", {"data": payload(gen=5.5)}, {})
add("attestation_generation", {"data": payload(gen=None)}, {})
add("attestation_generation", {"data": payload(gen="")}, {})
add("attestation_generation", {"data": payload(gen=0)}, {})
add("attestation_generation", {"data": {"generation": True}}, {})
add("attestation_generation", {"data": {}}, {})
add("attestation_generation", {"data": None}, {})
add("attestation_generation", {"data": 5}, {})

# ── _attested_create_time ──
CT = {"create_times": {"5": 100, "6": None, "7": True, "8": "x", "9": 2.5}}
add("attested_create_time", {"data": CT, "pid": 5}, {})
add("attested_create_time", {"data": CT, "pid": 6}, {})
add("attested_create_time", {"data": CT, "pid": 7}, {})
add("attested_create_time", {"data": CT, "pid": 8}, {})
add("attested_create_time", {"data": CT, "pid": 9}, {})
add("attested_create_time", {"data": CT, "pid": 10}, {})
add("attested_create_time", {"data": {"create_times": [1]}, "pid": 5}, {})
add("attested_create_time", {"data": None, "pid": 5}, {})
add("attested_create_time", {"data": {"create_times": {"123456": 7}}, "pid": 123456}, {})

# ── _read_start_attestation ──
add("read_start_attestation", {"home": "/h"}, marker_seams(payload()))
add("read_start_attestation", {"home": None}, marker_seams(payload()))
add("read_start_attestation", {"home": "/h"},
    {"start_attestation_path": MARKER, "path_read_text": "[1, 2, 3]"})
add("read_start_attestation", {"home": "/h"},
    {"start_attestation_path": MARKER, "path_read_text": '"just a string"'})
add("read_start_attestation", {"home": "/h"},
    {"start_attestation_path": MARKER, "path_read_text": "42"})
add("read_start_attestation", {"home": "/h"},
    {"start_attestation_path": MARKER, "path_read_text": "null"})
add("read_start_attestation", {"home": "/h"},
    {"start_attestation_path": MARKER, "path_read_text": "{bad json"})
add("read_start_attestation", {"home": "/h"},
    {"start_attestation_path": MARKER, "path_read_text": ""})
add("read_start_attestation", {"home": "/h"},
    {"start_attestation_path": MARKER, "path_read_text": "[1,]"})
add("read_start_attestation", {"home": "/h"},
    {"start_attestation_path": MARKER, "path_read_text": raise_spec("FileNotFoundError", "gone")})
add("read_start_attestation", {"home": "/h"},
    {"start_attestation_path": MARKER, "path_read_text": raise_spec("PermissionError", "no")})
add("read_start_attestation", {"home": "/h"},
    {"start_attestation_path": MARKER, "path_read_text": raise_spec("RuntimeError", "boom")})
add("read_start_attestation", {"home": "/h"},
    {"start_attestation_path": raise_spec("ValueError", "bad path")})
add("read_start_attestation", {"home": "/h"},
    {"start_attestation_path": raise_spec("RuntimeError", "path boom")})
add("read_start_attestation", {"home": "/h"},
    {"start_attestation_path": MARKER, "path_read_text": '{"a": 1, "a": 2}'})
add("read_start_attestation", {"home": "/h"},
    {"start_attestation_path": MARKER, "path_read_text": '{"a": 01}'})
add("read_start_attestation", {"home": "/h"},
    {"start_attestation_path": MARKER, "path_read_text": None})
add("read_start_attestation", {"home": "/h"},
    {"start_attestation_path": MARKER, "path_read_text": 5})

# ── _clear_start_attestation ──
add("clear_start_attestation", {"home": "/h"},
    {"start_attestation_path": MARKER, "path_unlink": None})
add("clear_start_attestation", {"home": None},
    {"start_attestation_path": MARKER, "path_unlink": None})
add("clear_start_attestation", {"home": "/h"},
    {"start_attestation_path": MARKER, "path_unlink": raise_spec("FileNotFoundError", "x")})
add("clear_start_attestation", {"home": "/h"},
    {"start_attestation_path": MARKER, "path_unlink": raise_spec("PermissionError", "x")})
add("clear_start_attestation", {"home": "/h"},
    {"start_attestation_path": MARKER, "path_unlink": raise_spec("RuntimeError", "x")})
add("clear_start_attestation", {"home": "/h"},
    {"start_attestation_path": raise_spec("OSError", "x")})
add("clear_start_attestation", {"home": "/h"},
    {"start_attestation_path": raise_spec("RuntimeError", "x")})
add("clear_start_attestation", {"home": "/h"},
    {"start_attestation_path": MARKER, "path_unlink": raise_spec("KeyboardInterrupt", "stop")})

# ── _attestation_within_horizon ──
def horizon(data, now=NOW + 100, **over):
    s = {"time_time": now}
    s.update(over)
    add("attestation_within_horizon", {"data": data}, s)

horizon(payload(ts=TS_FRESH))                       # age 100 -> True
horizon(payload(ts=TS_NAIVE))                       # naive -> UTC, age 100
horizon(payload(ts="2026-11-18"))                   # date-only midnight, age ~45296
horizon(payload(ts="2026-11-16T12:34:56+00:00"))    # age 172900 > 86400 -> False
horizon(payload(ts="2026-11-19T12:34:56+00:00"))    # age -86300 < -60 -> False
horizon(payload(ts="2026-11-18T12:36:56+00:00"))    # age -20 within slack -> True
horizon(payload(ts="2026-11-18T12:34:56.500000+00:00"), now=NOW)  # age -0.5 -> True
horizon(payload(ts=TS_FRESH), now=NOW + 86400)      # age exactly 86400 -> True
horizon(payload(ts=TS_FRESH), now=NOW - 60)         # age exactly -60 -> True
horizon(payload(ts=TS_FRESH), now=NOW + 86401)      # False
horizon(payload(ts=TS_FRESH), now=NOW - 61)         # False
horizon(payload(ts="not-a-date"))
horizon(payload(ts=None))
horizon(payload(ts=5))
horizon({"ts": TS_FRESH})
horizon({"pids": [1]})                              # missing ts -> False
horizon(None)
horizon([1])
horizon("x")
horizon(payload(ts="2026-02-29T00:00:00"))          # invalid day -> False
horizon(payload(ts="2026-11-18T12:34:56+05:30"), now=1794985496 + 10)  # offset ts
horizon(payload(ts=TS_FRESH), **{"time_time": raise_spec("RuntimeError", "clock")})
horizon(payload(ts=TS_FRESH), **{"time_time": raise_spec("KeyboardInterrupt", "stop")})

# ── _write_start_attestation ──
def write_seams(**over):
    s = {
        "start_attestation_path": MARKER,
        "path_mkdir": None,
        "import_process_create_time": None,
        "datetime_now": TS_FRESH,
        "uuid4_hex": "gen0001",
        "process_create_time": [111, 222],
        "path_write_text": None,
        "path_replace": None,
    }
    s.update(over)
    return s

add("write_start_attestation", {"pids": [123, 456], "via": "direct spawn", "home": "/h"}, write_seams())
add("write_start_attestation", {"pids": [], "via": "schtasks", "home": None}, write_seams())
add("write_start_attestation", {"pids": [7], "via": "x", "home": "/h"},
    write_seams(process_create_time=[None]))
add("write_start_attestation", {"pids": [7, 7], "via": "x", "home": "/h"},
    write_seams(process_create_time=[111, 222]))
add("write_start_attestation", {"pids": [5.9, 8], "via": "x", "home": "/h"},
    write_seams(process_create_time=[111, 222]))
add("write_start_attestation", {"pids": ["7", 8], "via": "x", "home": "/h"},
    write_seams(process_create_time=[111, 222]))
add("write_start_attestation", {"pids": [True], "via": "x", "home": "/h"},
    write_seams(process_create_time=[111]))
add("write_start_attestation", {"pids": ["x"], "via": "x", "home": "/h"}, write_seams())
add("write_start_attestation", {"pids": [1], "via": "x", "home": "/h"},
    write_seams(path_mkdir=raise_spec("PermissionError", "no")))
add("write_start_attestation", {"pids": [1], "via": "x", "home": "/h"},
    write_seams(import_process_create_time=raise_spec("ImportError", "no mod")))
add("write_start_attestation", {"pids": [1], "via": "x", "home": "/h"},
    write_seams(process_create_time=[raise_spec("OSError", "dead")]))
add("write_start_attestation", {"pids": [1], "via": "x", "home": "/h"},
    write_seams(path_write_text=raise_spec("OSError", "disk")))
add("write_start_attestation", {"pids": [1], "via": "x", "home": "/h"},
    write_seams(path_replace=raise_spec("PermissionError", "locked")))
add("write_start_attestation", {"pids": [1], "via": "x", "home": "/h"},
    write_seams(datetime_now=raise_spec("RuntimeError", "clock")))
add("write_start_attestation", {"pids": [1], "via": "x", "home": "/h"},
    write_seams(datetime_now=raise_spec("KeyboardInterrupt", "stop")))
add("write_start_attestation", {"pids": [1], "via": "x", "home": "/h"},
    write_seams(datetime_now=None, uuid4_hex=None, process_create_time=[None]))
add("write_start_attestation", {"pids": [1], "via": "x", "home": "/h"},
    write_seams(start_attestation_path=raise_spec("RuntimeError", "path")))

# ── check_start_attestation ──
def check_seams(data, dead=True, **over):
    s = marker_seams(data)
    s["attested_dead"] = dead
    s["import_find_gateway_pids"] = None
    s["find_gateway_pids"] = [[]]
    s["is_task_registered"] = False
    s["get_task_name"] = "Hermes_Gateway"
    s["print"] = None
    s.update(over)
    return s

add("check_start_attestation", {"current_pids": None}, check_seams(None))
add("check_start_attestation", {"current_pids": None}, check_seams(5))
add("check_start_attestation", {"current_pids": None}, check_seams(payload(pids=[])))
add("check_start_attestation", {"current_pids": None}, check_seams(payload(pids=[True])))
add("check_start_attestation", {"current_pids": None}, check_seams(payload(), dead=True))
add("check_start_attestation", {"current_pids": None}, check_seams(payload(), dead=False))
add("check_start_attestation", {"current_pids": []}, check_seams(payload(), dead=True))
add("check_start_attestation", {"current_pids": [999]}, check_seams(payload(), dead=False))
add("check_start_attestation", {"current_pids": None},
    check_seams(payload(), import_find_gateway_pids=raise_spec("ImportError", "no")))
add("check_start_attestation", {"current_pids": None},
    check_seams(payload(), find_gateway_pids=raise_spec("RuntimeError", "scan")))
add("check_start_attestation", {"current_pids": None},
    check_seams(payload(), find_gateway_pids=5))
add("check_start_attestation", {"current_pids": None},
    check_seams(payload(), find_gateway_pids=[[11, 22]], dead=True, is_task_registered=True))
add("check_start_attestation", {"current_pids": None},
    check_seams(payload(), find_gateway_pids=None))
add("check_start_attestation", {"current_pids": None},
    check_seams(payload(), attested_dead=raise_spec("RuntimeError", "dead boom")))
add("check_start_attestation", {"current_pids": None},
    check_seams(payload(), path_read_text=raise_spec("RuntimeError", "read boom")))
add("check_start_attestation", {"current_pids": None},
    check_seams(payload(), attested_dead=raise_spec("SystemExit", "bye")))

# ── _format_attestation_warning ──
def fmt_seams(registered=False, name="Hermes_Gateway"):
    return {"is_task_registered": registered, "get_task_name": name}

add("format_attestation_warning", {"attested": [123, 456], "data": payload()}, fmt_seams())
add("format_attestation_warning", {"attested": [123], "data": payload()}, fmt_seams(True))
add("format_attestation_warning", {"attested": [1], "data": payload(via=None)}, fmt_seams(True))
add("format_attestation_warning", {"attested": [1], "data": payload(via="")}, fmt_seams())
add("format_attestation_warning", {"attested": [1], "data": payload(ts=None)}, fmt_seams(True))
add("format_attestation_warning", {"attested": [1], "data": payload(ts="")}, fmt_seams())
add("format_attestation_warning", {"attested": [1], "data": {"via": "x", "ts": "t"}}, fmt_seams(True))
add("format_attestation_warning", {"attested": [1], "data": payload(via=5, ts=7)}, fmt_seams())
add("format_attestation_warning", {"attested": [1], "data": payload()},
    {"is_task_registered": raise_spec("RuntimeError", "reg")})
add("format_attestation_warning", {"attested": [1], "data": payload()},
    {"is_task_registered": True, "get_task_name": raise_spec("OSError", "name")})

# ── _task_run_hint ──
FMT = "  Recovery: schtasks /Run /TN {}   (Task Scheduler starts the gateway outside any Job Object)"
add("task_run_hint", {"fmt": FMT}, {"is_task_registered": True, "get_task_name": "Hermes_Gateway"})
add("task_run_hint", {"fmt": FMT}, {"is_task_registered": False})
add("task_run_hint", {"fmt": "no placeholder"}, {"is_task_registered": True, "get_task_name": "T"})
add("task_run_hint", {"fmt": "{0}"}, {"is_task_registered": True, "get_task_name": "T"})
add("task_run_hint", {"fmt": "{bad}"}, {"is_task_registered": True, "get_task_name": "T"})
add("task_run_hint", {"fmt": "{{esc}} {}"}, {"is_task_registered": True, "get_task_name": "T"})
add("task_run_hint", {"fmt": FMT}, {"is_task_registered": raise_spec("RuntimeError", "x")})
add("task_run_hint", {"fmt": FMT}, {"is_task_registered": True, "get_task_name": raise_spec("OSError", "x")})
add("task_run_hint", {"fmt": FMT}, {"is_task_registered": raise_spec("KeyboardInterrupt", "stop")})

# ── _print_task_run_hint ──
add("print_task_run_hint", {"fmt": FMT}, {"is_task_registered": True, "get_task_name": "T", "print": None})
add("print_task_run_hint", {"fmt": FMT}, {"is_task_registered": False, "print": None})
add("print_task_run_hint", {"fmt": FMT}, {"is_task_registered": raise_spec("OSError", "x"), "print": None})
add("print_task_run_hint", {"fmt": FMT},
    {"is_task_registered": True, "get_task_name": "T", "print": raise_spec("RuntimeError", "p")})
add("print_task_run_hint", {"fmt": FMT},
    {"is_task_registered": True, "get_task_name": "T", "print": raise_spec("KeyboardInterrupt", "s")})

# ── _print_start_attestation_warning ──
add("print_start_attestation_warning", {}, check_seams(None))
add("print_start_attestation_warning", {}, check_seams(payload(), dead=True))
add("print_start_attestation_warning", {}, check_seams(payload(), dead=False))
add("print_start_attestation_warning", {}, check_seams(payload(), dead=True, is_task_registered=True))
add("print_start_attestation_warning", {},
    check_seams(payload(), path_read_text=raise_spec("RuntimeError", "swallowed")))
add("print_start_attestation_warning", {},
    check_seams(payload(), attested_dead=raise_spec("KeyboardInterrupt", "stop")))
add("print_start_attestation_warning", {},
    check_seams(payload(), dead=True, print=raise_spec("OSError", "print fail")))

# ── _query_scheduled_task_xml ──
def exec_script(code=0, out="", err=""):
    return {"code": code, "out": out, "err": err}

add("query_scheduled_task_xml", {"task_name": "Hermes_Gateway"},
    {"exec_schtasks": exec_script(0, "<Task/>", "")})
add("query_scheduled_task_xml", {"task_name": "T"},
    {"exec_schtasks": exec_script(0, "  <Task/>  \n", "")})
add("query_scheduled_task_xml", {"task_name": "T"},
    {"exec_schtasks": exec_script(1, "", "ERROR: access denied")})
add("query_scheduled_task_xml", {"task_name": "T"},
    {"exec_schtasks": exec_script(0, "   ", "")})
add("query_scheduled_task_xml", {"task_name": "T"},
    {"exec_schtasks": exec_script(0, "", "no output")})
add("query_scheduled_task_xml", {"task_name": "T"},
    {"exec_schtasks": raise_spec("OSError", "spawn fail")})
add("query_scheduled_task_xml", {"task_name": "T"},
    {"exec_schtasks": raise_spec("KeyboardInterrupt", "stop")})

# ── _task_xml_leaf_values ──
XML_CASES = {
    "simple": '<Task version="1.2"><Settings><Enabled>true</Enabled></Settings></Task>',
    "ns": '<Task xmlns="http://schemas.microsoft.com/windows/2004/02/mit/task" version="1.4"><Actions><Exec><Command>python.exe</Command></Exec></Actions></Task>',
    "prefixed": '<p:Task xmlns:p="urn:x" version="1.2"><p:Settings><p:Interval>PT1M</p:Interval></p:Settings></p:Task>',
    "prefixed_attr": '<Task xmlns:p="urn:x" p:version="9" version="1"><A/></Task>',
    "selfclose": '<Task version="1.2"><A/></Task>',
    "ws_text": '<Task><A>  hello   world  </A></Task>',
    "entities": '<Task><A>a&amp;b&lt;c&gt;d&quot;e&apos;f&#65;&#x42;</A></Task>',
    "comment": '<Task><!-- hi --><A>x</A><!-- tail --></Task>',
    "cdata": '<Task><A><![CDATA[raw <&> text]]></A></Task>',
    "xmldecl": '<?xml version="1.0" encoding="UTF-8"?><Task version="1.2"><A/></Task>',
    "pi": '<?target data?><Task><A/></Task>',
    "dup_leaf": '<Task><A>1</A><A>2</A></Task>',
    "empty_text": '<Task><A></A><B/></Task>',
    "nested_same": '<Task><A><B>1</B></A><A><B>2</B></A></Task>',
    "attr_ws": '<Task  version = "1.2" ><A/></Task>',
    "endtag_ws": '<Task><A>x</A ></Task>',
    "tail_text": '<Task><A>x</A>tail<B>y</B></Task>',
    "mixed": '<Task><A>x<B/>y</A></Task>',
    "bad_mismatch": '<Task><A></B></Task>',
    "bad_trunc": '<Task><A>',
    "bad_junk": '<Task/><junk/>',
    "bad_trail_text": '<Task/>junk',
    "bad_dup_attr": '<Task version="1" version="2"/>',
    "bad_unquoted": '<Task version=1/>',
    "bad_bare_amp": '<Task><A>a&b</A></Task>',
    "bad_unknown_entity": '<Task><A>&foo;</A></Task>',
    "bad_lt_attr": '<Task version="a<b"/>',
    "doctype": '<!DOCTYPE Task><Task/>',
    "bom": '\ufeff<Task/>',
    "bad_empty": '',
    "bad_ws_only": '   ',
    "bad_name_digit": '<1Task/>',
    "bad_unbound_prefix": '<p:Task xmlns:q="urn:x"><A/></p:Task>',
    "bad_close_prefix_diff": '<p:Task xmlns:p="urn:x" xmlns:q="urn:x"><A/></q:Task>',
    "bad_comment_dash": '<Task><!-- a--b --><A/></Task>',
    "bad_cdata_end": '<Task><A>]]></A></Task>',
    "numeric_zero": '<Task><A>&#0;</A></Task>',
    "numeric_surrogate": '<Task><A>&#xD800;</A></Task>',
    "numeric_big": '<Task><A>&#x110000;</A></Task>',
    "numeric_nonascii": '<Task><A>&#233;</A></Task>',
    "attr_entity": '<Task version="a&amp;b"><A/></Task>',
    "attr_newline": '<Task version="a\nb"><A/></Task>',
    "attr_tab": '<Task version="a\tb"><A/></Task>',
    "deep": '<Task><Principals><Principal><LogonType>InteractiveToken</LogonType></Principal></Principals></Task>',
    "no_version": '<Task><A/></Task>',
    "empty_root": '<Task/>',
    "root_text": '<Task>  </Task>',
    "single_quote_attr": "<Task version='1.2'><A/></Task>",
    "trailing_ws": '<Task/>  \n',
    "trailing_comment": '<Task/><!-- ok -->',
    "xmldecl_not_start": ' <?xml version="1.0"?><Task/>',
    "double_hyphen_end": '<Task><!-- a---><A/></Task>',
    "xmlns_rebind": '<Task xmlns="urn:a"><A xmlns="urn:b"><B/></A></Task>',
    "comment_inside_text": '<Task><A>x<!--c-->y</A></Task>',
    "text_cr": '<Task><A>a\rb</A></Task>',
    "crlf_text": '<Task><A>a\r\nb</A></Task>',
    "deep_ns_attr": '<p:Task xmlns:p="urn:x" p:version="3" version="1"><p:A/></p:Task>',
    "attr_numeric_nl": '<Task version="a&#10;b"><A/></Task>',
    "pi_inside": '<Task><?pi data?><A/></Task>',
    "bad_text_decl": '<?xml encoding="UTF-8"?><Task/>',
    "bad_nested_comment": '<Task><!--<!--x--><A/></Task>',
    "empty_attr": '<Task version=""><A/></Task>',
    "xmlns_empty": '<Task xmlns=""><A/></Task>',
    "colon_name": '<a:b xmlns:a="urn:x"><c/></a:b>',
    "attr_gt": '<Task version="a>b"><A/></Task>',
    "full_task": ('<?xml version="1.0" encoding="UTF-16"?>\n'
                  '<Task version="1.2" xmlns="http://schemas.microsoft.com/windows/2004/02/mit/task">\n'
                  '  <Principals><Principal id="Author"><LogonType>InteractiveToken</LogonType>'
                  '<RunLevel>LeastPrivilege</RunLevel></Principal></Principals>\n'
                  '  <Settings><RestartOnFailure><Interval>PT1M</Interval><Count>999</Count></RestartOnFailure></Settings>\n'
                  '  <Triggers><LogonTrigger><Delay>PT30S</Delay></LogonTrigger></Triggers>\n'
                  '  <Actions><Exec><Command>wscript.exe</Command><Arguments>"C:\\h\\gateway.vbs"</Arguments></Exec></Actions>\n'
                  '</Task>'),
}
for k, xml in XML_CASES.items():
    add("task_xml_leaf_values", {"xml": xml}, {})

# ── compare_scheduled_task_drift ──
TPL = ('<Task version="1.2"><Settings><RestartOnFailure><Interval>PT1M</Interval></RestartOnFailure></Settings>'
       '<Triggers><LogonTrigger><Delay>PT30S</Delay></LogonTrigger></Triggers>'
       '<Actions><Exec><Arguments>"C:\\h\\g.vbs"</Arguments></Exec></Actions>'
       '<Principals><Principal><LogonType>InteractiveToken</LogonType></Principal></Principals></Task>')
REG_ALIGNED = TPL
REG_OLD = ('<Task version="1.1"><Actions><Exec><Arguments>"C:\\h\\old.vbs"</Arguments></Exec></Actions></Task>')
REG_S4U = ('<Task version="1.2"><Settings><RestartOnFailure><Interval>PT1M</Interval></RestartOnFailure></Settings>'
           '<Triggers><LogonTrigger><Delay>PT30S</Delay></LogonTrigger></Triggers>'
           '<Actions><Exec><Arguments>"C:\\h\\g.vbs"</Arguments></Exec></Actions>'
           '<Principals><Principal><LogonType>S4U</LogonType></Principal></Principals></Task>')
add("compare_scheduled_task_drift", {"registered_xml": REG_ALIGNED, "template_xml": TPL}, {})
add("compare_scheduled_task_drift", {"registered_xml": REG_OLD, "template_xml": TPL}, {})
add("compare_scheduled_task_drift", {"registered_xml": REG_S4U, "template_xml": TPL}, {})
add("compare_scheduled_task_drift", {"registered_xml": "<bad", "template_xml": TPL}, {})
add("compare_scheduled_task_drift", {"registered_xml": TPL, "template_xml": "<bad"}, {})
add("compare_scheduled_task_drift", {"registered_xml": "<bad", "template_xml": "<bad"}, {})
add("compare_scheduled_task_drift",
    {"registered_xml": '<Task version="1.2"/>', "template_xml": TPL}, {})
add("compare_scheduled_task_drift",
    {"registered_xml": '<Task version="1.2"><Principals><Principal><LogonType></LogonType></Principal></Principals></Task>',
     "template_xml": TPL}, {})
add("compare_scheduled_task_drift",
    {"registered_xml": '<Task version="1.3"><Settings><RestartOnFailure><Interval>PT2M</Interval></RestartOnFailure></Settings>'
                       '<Triggers><LogonTrigger><Delay>PT30S</Delay></LogonTrigger></Triggers>'
                       '<Actions><Exec><Arguments>"C:\\h\\g.vbs"</Arguments></Exec></Actions></Task>',
     "template_xml": TPL}, {})

# ── scheduled_task_drift ──
def drift_seams(reg_xml, tpl=TPL, **over):
    s = {
        "exec_schtasks": exec_script(0, reg_xml, "") if reg_xml is not None else exec_script(1, "", "err"),
        "get_task_script_path": "/h/gateway-service/g.cmd",
        "resolve_task_user": "DOMAIN\\user",
        "build_scheduled_task_xml": tpl,
    }
    s.update(over)
    return s

add("scheduled_task_drift", {"task_name": "Hermes_Gateway"}, drift_seams(REG_ALIGNED))
add("scheduled_task_drift", {"task_name": "Hermes_Gateway"}, drift_seams(REG_OLD))
add("scheduled_task_drift", {"task_name": "Hermes_Gateway"}, drift_seams(None))
add("scheduled_task_drift", {"task_name": "Hermes_Gateway"}, drift_seams(REG_S4U))
add("scheduled_task_drift", {"task_name": "Hermes_Gateway"},
    drift_seams(REG_ALIGNED, resolve_task_user=None))
add("scheduled_task_drift", {"task_name": "Hermes_Gateway"},
    drift_seams(REG_ALIGNED, build_scheduled_task_xml=raise_spec("RuntimeError", "tpl")))
add("scheduled_task_drift", {"task_name": "Hermes_Gateway"},
    drift_seams(REG_ALIGNED, get_task_script_path=raise_spec("OSError", "path")))

# ── _print_scheduled_task_drift ──
add("print_scheduled_task_drift", {"task_name": "T"}, dict(drift_seams(REG_ALIGNED), print=None))
add("print_scheduled_task_drift", {"task_name": "T"}, dict(drift_seams(REG_OLD), print=None))
add("print_scheduled_task_drift", {"task_name": "T"}, dict(drift_seams(None), print=None))
add("print_scheduled_task_drift", {"task_name": "T"}, dict(drift_seams(REG_S4U), print=None))
add("print_scheduled_task_drift", {"task_name": "T"},
    dict(drift_seams(REG_OLD), print=raise_spec("RuntimeError", "p")))
add("print_scheduled_task_drift", {"task_name": "T"},
    drift_seams(REG_ALIGNED, exec_schtasks=raise_spec("OSError", "exec")))

# ── registered_task_launchers ──
LISTING = ('"\\Hermes_Gateway","N/A","Ready"\r\n'
           '"\\Hermes_Gateway_work","N/A","Ready"\r\n'
           '"\\Hermes_Gateway","dup","X"\r\n'
           '"\\Other","x","y"\r\n')
TASK_XML = ('<Task version="1.2"><Actions><Exec><Command>wscript.exe</Command>'
            '<Arguments>"C:\\h\\g.vbs"</Arguments></Exec></Actions></Task>')
TASK_XML2 = ('<Task version="1.2"><Actions><Exec><Command>"C:\\py\\python.exe"</Command>'
             '<Arguments>-m hermes_cli.main gateway run</Arguments></Exec></Actions></Task>')

def reg_seams(listing=LISTING, code=0, xmls=None, **over):
    xmls = xmls if xmls is not None else [TASK_XML, TASK_XML2]
    # entries may be raw XML strings or pre-built exec result dicts
    wrapped = [x if isinstance(x, dict) else exec_script(0, x, "") for x in xmls]
    s = {
        "import_service_definition_launchers": None,
        "exec_schtasks": [exec_script(code, listing, "")] + wrapped,
        "service_definition_launchers": [["/h/g.vbs"], ["/h/other.vbs"]],
    }
    s.update(over)
    return s

add("registered_task_launchers", {"platform": "linux"}, {})
add("registered_task_launchers", {"platform": "win32"}, reg_seams())
add("registered_task_launchers", {"platform": "win32"}, reg_seams(code=1))
add("registered_task_launchers", {"platform": "win32"}, reg_seams(listing=""))
add("registered_task_launchers", {"platform": "win32"},
    reg_seams(xmls=[exec_script(1, "", "denied"), TASK_XML2]))
add("registered_task_launchers", {"platform": "win32"},
    reg_seams(xmls=["<bad xml", TASK_XML2]))
add("registered_task_launchers", {"platform": "win32"},
    reg_seams(xmls=['<Task version="1.2"/>', TASK_XML2]))
add("registered_task_launchers", {"platform": "win32"},
    reg_seams(service_definition_launchers=raise_spec("RuntimeError", "svc")))
add("registered_task_launchers", {"platform": "win32"},
    reg_seams(import_service_definition_launchers=raise_spec("ImportError", "no")))
add("registered_task_launchers", {"platform": "win32"},
    reg_seams(exec_schtasks=raise_spec("OSError", "exec")))
add("registered_task_launchers", {"platform": "win32"},
    reg_seams(listing='"\\Hermes_Gateway_unterminated\r\n"\\Hermes_Gateway","a","b"\n',
              xmls=[TASK_XML, TASK_XML2]))
add("registered_task_launchers", {"platform": "win32"},
    reg_seams(listing='"\\Hermes_Gateway_bad\\name","x","y"\r\n"\\Hermes_GatewayX","x","y"\r\n',
              xmls=[TASK_XML]))
add("registered_task_launchers", {"platform": "win32"},
    reg_seams(xmls=[{"code": 0, "out": {"not": "xml"}, "err": ""}, TASK_XML2]))

# ── _launcher_owned_exit_line ──
add("launcher_owned_exit_line", {"hermes_home": "/h"},
    {"import_launcher_pid_file": None, "launcher_pid_file": "/h/gateway.pid",
     "quote_vbs_string": '"/h/gateway.pid"'})
add("launcher_owned_exit_line", {"hermes_home": "/h2"},
    {"import_launcher_pid_file": None, "launcher_pid_file": "/h2/gateway.pid",
     "quote_vbs_string": '"/h2/gateway.pid"'})
add("launcher_owned_exit_line", {"hermes_home": "/h"},
    {"import_launcher_pid_file": None, "launcher_pid_file": "/h/g.pid",
     "quote_vbs_string": raise_spec("ValueError", "newline")})
add("launcher_owned_exit_line", {"hermes_home": "/h"},
    {"import_launcher_pid_file": None, "launcher_pid_file": raise_spec("OSError", "pid")})
add("launcher_owned_exit_line", {"hermes_home": "/h"},
    {"import_launcher_pid_file": raise_spec("ImportError", "no")})
add("launcher_owned_exit_line", {"hermes_home": "/h"},
    {"import_launcher_pid_file": None, "launcher_pid_file": "/h/g.pid", "quote_vbs_string": None})

# ── _launcher_pythonpath_entries ──
add("launcher_pythonpath_entries", {"extra_pythonpath": []},
    {"project_root_str": "/repo", "preserve_hermes_home_path": "/repo"})
add("launcher_pythonpath_entries", {"extra_pythonpath": ["/a", "/b"]},
    {"project_root_str": "/repo", "preserve_hermes_home_path": ["/repo", "/a", "/b"]})
add("launcher_pythonpath_entries", {"extra_pythonpath": ["/a"]},
    {"project_root_str": "/repo", "preserve_hermes_home_path": raise_spec("RuntimeError", "p")})
add("launcher_pythonpath_entries", {"extra_pythonpath": []},
    {"project_root_str": raise_spec("OSError", "root")})
add("launcher_pythonpath_entries", {"extra_pythonpath": []},
    {"project_root_str": None})
add("launcher_pythonpath_entries", {"extra_pythonpath": ["/a"]},
    {"project_root_str": "/repo", "preserve_hermes_home_path": [None, "/a"]})

# ── _render_launchers ──
SETTINGS = ["C:/py/python.exe", "/h", "/h", "--profile work"]
add("render_launchers", {"settings": SETTINGS, "script_path": "/h/gateway-service/g.cmd"},
    {"build_gateway_cmd_script": "@echo off\r\n", "build_gateway_vbs_script": "' vbs\r\n"})
add("render_launchers", {"settings": SETTINGS, "script_path": "/h/g.cmd"},
    {"build_gateway_cmd_script": "CMD", "build_gateway_vbs_script": "VBS"})
add("render_launchers", {"settings": SETTINGS, "script_path": "/h/g.cmd"},
    {"build_gateway_cmd_script": raise_spec("ValueError", "quote"), "build_gateway_vbs_script": "VBS"})
add("render_launchers", {"settings": SETTINGS, "script_path": "/h/g.cmd"},
    {"build_gateway_cmd_script": "CMD", "build_gateway_vbs_script": raise_spec("RuntimeError", "vbs")})
add("render_launchers", {"settings": SETTINGS, "script_path": None},
    {"build_gateway_cmd_script": "CMD", "build_gateway_vbs_script": "VBS"})
add("render_launchers", {"settings": ["p", "w", "h", ""], "script_path": "/h/g.cmd"},
    {"build_gateway_cmd_script": None, "build_gateway_vbs_script": None})

# ── _write_launcher_scripts ──
def atomic_seams(**over):
    s = {"path_write_text": None, "path_replace": None, "path_unlink": None}
    s.update(over)
    return s

add("write_launcher_scripts",
    {"scripts": [["/h/g.cmd", "@echo off\r\n", "utf-8"], ["/h/g.vbs", "' vbs\r\n", "utf-16"]]},
    atomic_seams())
add("write_launcher_scripts", {"scripts": []}, atomic_seams())
add("write_launcher_scripts", {"scripts": [["/h/g.cmd", "C", "utf-8"]]},
    atomic_seams(path_write_text=raise_spec("OSError", "disk")))
add("write_launcher_scripts", {"scripts": [["/h/g.vbs", "V", "utf-16"]]},
    atomic_seams(path_replace=raise_spec("PermissionError", "lock")))
add("write_launcher_scripts", {"scripts": [[None, "C", "utf-8"]]}, atomic_seams())
add("write_launcher_scripts", {"scripts": [[None, "V", "utf-16"]]}, atomic_seams())
add("write_launcher_scripts", {"scripts": [["/h/g.cmd", "C", "utf-8"]]},
    atomic_seams(path_unlink=raise_spec("OSError", "stale")))

# ── _atomic_write ──
add("atomic_write", {"path": "/h/g.cmd", "content": "C", "tmp": "/h/g.tmp", "encoding": "utf-8"},
    atomic_seams())
add("atomic_write", {"path": "/h/g.vbs", "content": "V", "tmp": "/h/g.vbs.tmp", "encoding": "utf-16"},
    atomic_seams())
add("atomic_write", {"path": "/h/g.cmd", "content": "C", "tmp": "/h/g.tmp", "encoding": "utf-8"},
    atomic_seams(path_write_text=raise_spec("OSError", "disk")))
add("atomic_write", {"path": "/h/g.cmd", "content": "C", "tmp": "/h/g.tmp", "encoding": "utf-8"},
    atomic_seams(path_replace=raise_spec("PermissionError", "lock")))
add("atomic_write", {"path": "/h/g.cmd", "content": "C", "tmp": "/h/g.tmp", "encoding": "utf-8"},
    atomic_seams(path_unlink=raise_spec("OSError", "stale")))
add("atomic_write", {"path": "/h/g.cmd", "content": "C", "tmp": "/h/g.tmp", "encoding": "utf-8"},
    atomic_seams(path_unlink=raise_spec("RuntimeError", "weird")))
add("atomic_write", {"path": "/h/g.cmd", "content": "C", "tmp": "/h/g.tmp", "encoding": "utf-8"},
    atomic_seams(path_write_text=raise_spec("OSError", "disk"),
                 path_unlink=raise_spec("RuntimeError", "weird")))
add("atomic_write", {"path": "/h/g.cmd", "content": "C", "tmp": "/h/g.tmp", "encoding": "utf-8"},
    atomic_seams(path_write_text=raise_spec("KeyboardInterrupt", "stop")))
add("atomic_write", {"path": "/h/g.cmd", "content": "C", "tmp": "/h/g.tmp", "encoding": "utf-8"},
    atomic_seams(path_replace=raise_spec("KeyboardInterrupt", "stop"),
                 path_unlink=raise_spec("OSError", "stale")))

# ── _report_already_running ──
add("report_already_running", {"pids": [123, 456]}, {"print": None})
add("report_already_running", {"pids": []}, {"print": None})
add("report_already_running", {"pids": [1]}, {"print": None})
add("report_already_running", {"pids": [1, "x", None]}, {"print": None})
add("report_already_running", {"pids": [1]}, {"print": raise_spec("OSError", "console")})
add("report_already_running", {"pids": [1]}, {"print": raise_spec("KeyboardInterrupt", "stop")})

# ── _startup_staging_path ──
add("startup_staging_path", {}, {"get_startup_entry_path": "/startup/Hermes_Gateway.vbs"})
add("startup_staging_path", {}, {"get_startup_entry_path": "/startup/Hermes_Gateway.cmd"})
add("startup_staging_path", {}, {"get_startup_entry_path": "/startup/noext"})
add("startup_staging_path", {}, {"get_startup_entry_path": None})
add("startup_staging_path", {}, {"get_startup_entry_path": raise_spec("RuntimeError", "no windows")})
add("startup_staging_path", {}, {"get_startup_entry_path": raise_spec("KeyboardInterrupt", "stop")})

# ── _hermes_home ──
add("hermes_home", {}, {"import_get_hermes_home": None, "get_hermes_home": "/h"})
add("hermes_home", {}, {"import_get_hermes_home": None, "get_hermes_home": "/h/profiles/work"})
add("hermes_home", {}, {"import_get_hermes_home": None, "get_hermes_home": ""})
add("hermes_home", {}, {"import_get_hermes_home": None, "get_hermes_home": None})
add("hermes_home", {}, {"import_get_hermes_home": raise_spec("ImportError", "no config")})
add("hermes_home", {}, {"import_get_hermes_home": None, "get_hermes_home": raise_spec("RuntimeError", "home")})

# ── _mark_planned_restart ──
add("mark_planned_restart", {}, {"import_write_planned_restart_marker": None,
                                 "write_planned_restart_marker": None})
add("mark_planned_restart", {}, {"import_write_planned_restart_marker": None,
                                 "write_planned_restart_marker": raise_spec("OSError", "disk")})
add("mark_planned_restart", {}, {"import_write_planned_restart_marker": raise_spec("ImportError", "no")})
add("mark_planned_restart", {}, {"import_write_planned_restart_marker": None,
                                 "write_planned_restart_marker": raise_spec("KeyboardInterrupt", "stop")})
add("mark_planned_restart", {}, {"import_write_planned_restart_marker": None,
                                 "write_planned_restart_marker": True})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as fh:
        for c in CASES:
            fh.write(json.dumps(c, ensure_ascii=False) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    thin = {k: v for k, v in counts.items() if v < 5}
    print("wrote %d cases, %d funcs -> %s" % (len(CASES), len(counts), OUT))
    if thin:
        raise SystemExit("FATAL funcs with <5 cases: %s" % thin)
    for k in sorted(counts):
        print("  %-32s %d" % (k, counts[k]))


if __name__ == "__main__":
    main()