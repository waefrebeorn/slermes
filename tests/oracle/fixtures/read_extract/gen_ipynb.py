#!/usr/bin/env python3
import json, os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
nb = {
    "cells": [
        {"cell_type": "markdown", "source": ["# Title\n", "some prose"]},
        {"cell_type": "code", "source": ["x = 1\n", "y = 2"]},
        {"cell_type": "raw", "source": ["raw stuff"]},
    ],
    "metadata": {}, "nbformat": 4, "nbformat_minor": 5,
}
path = os.path.join(HERE, "sample.ipynb")
with open(path, "w") as f:
    json.dump(nb, f)
sys.path.insert(0, "/home/wubu/hermes-agent-dev")
from tools.read_extract import extract_document_text
with open(os.path.join(HERE, "sample.ipynb.ref.txt"), "w") as f:
    f.write(extract_document_text(path))
print("ipynb fixture + ref written")
