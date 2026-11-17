#!/usr/bin/env python3
"""Oracle for t_port_gateway_short_label.c.

_gateway_platform_short_label was deleted from hermes_cli/setup.py upstream;
its documented contract is reproduced inline so the C port still has a
reference. One line per input, same shape as the harness.
"""
import sys


def short_label(label):
    # base = label[:label.index("(")].strip(); if no paren or an empty base,
    # the original label is returned.
    paren = label.find("(")
    if paren < 0:
        return label
    base = label[:paren].strip()
    return base if base else label


def main():
    if len(sys.argv) < 2:
        sys.stderr.write("usage: sta_oracle_gateway_short_label.py <labels.txt>\n")
        return 2
    with open(sys.argv[1], "r", encoding="utf-8") as f:
        for line in f:
            label = line.rstrip("\n").rstrip("\r")
            sys.stdout.write(short_label(label) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
