# libwubuoffice — native slermes OOXML library

From-scratch OOXML toolkit written for slermes (by the project owner): a ZIP
reader/writer + DEFLATE (no zlib) and an OPC reader + WordprocessingML text
extractor. Lives directly in the slermes tree as `lib/libwubuoffice` alongside
the other `lib/lib*` components — no external dependency, no submodule, no
dependency pull. C11, no external libs.

Internal layout:
- `src/wubuzip/` — ZIP container + DEFLATE (inflater). Used by `wubuoxml_read`.
- `src/wubuoxml/` — OPC package reader (`wubuoxml_read`) + `.rels` graph
  parsing + WordprocessingML text extractor (`wubuoxml_docx_text`).
