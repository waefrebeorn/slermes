# Slermes oracle harness infrastructure
#
# This directory is the single home for faithful port-vs-LIVE-Python oracle
# checks. It exists to stop every port from reinventing its own fixtures,
# compile flags, and runner. Reuse it. Do NOT scatter fixtures in /tmp or
# recreate a per-port runner.
#
# Two oracle contracts are supported:
#
#   A) "piped" (legacy, run_one_oracle.sh)
#      The C harness (tests/t_port_<name>.c) prints oracle-readable lines on
#      stdout; tests/sta_oracle_<name>.py reads them on stdin and asserts
#      against LIVE Python. Run with: tests/run_one_oracle.sh <name>
#
#   B) "diff" (canonical-JSON, run_oracle.sh)
#      Both the C harness and the Python oracle emit the SAME canonical JSON
#      for a given fixture. run_oracle.sh compiles the harness, runs it and
#      the oracle over each fixture, and diffs the two outputs. Any diff =
#      mismatch. Used by structured ports (e.g. patch_parser).
#
# Fixtures live in fixtures/ and are committed. Add new ones there, not /tmp.

## fixtures/ layout
#
#   fixtures/<port_name>/
#       <case>.in          # raw input fed to both harness and oracle
#       ...
#
# The harness and oracle are told which fixture to read via argv[1].

## Adding a new oracle port
#
#   1. Write src/.../port_<name>.c + .h (faithful, single-line /* PoP: */).
#   2. Register the .o in build/objects.mk.
#   3. Write tests/t_port_<name>.c (reads fixture from argv[1], prints result).
#   4. Write tests/sta_oracle_<name>.py (reads same fixture, prints expected).
#   5. Drop fixtures in fixtures/<name>/.
#   6. Run: bash tests/oracle/runners/run_oracle.sh <name>
#   7. Wire into build/test.mk if you want it in `make test`.
