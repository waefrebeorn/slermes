#!/usr/bin/env python3
"""Generate real DOCX/XLSX/IPYNB fixtures (stdlib zipfile only) and emit the
Python reference extraction as ground truth for C-vs-Python parity check."""
import json, os, zipfile, sys

HERE = os.path.dirname(os.path.abspath(__file__))

def write_docx(path):
    document = (
        '<?xml version="1.0"?>'
        '<w:document xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main">'
        '<w:body>'
        '<w:p><w:r><w:t>Hello world</w:t></w:r></w:p>'
        '<w:p><w:r><w:t>This is a </w:t><w:tab/><w:t>tabbed</w:t><w:br/><w:t>and broken line.</w:t></w:r></w:p>'
        '<w:p><w:r><w:t>Unicode: café — 日本語</w:t></w:r></w:p>'
        '</w:body></w:document>'
    )
    content_types = (
        '<?xml version="1.0"?>'
        '<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">'
        '<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>'
        '<Default Extension="xml" ContentType="application/xml"/>'
        '<Override PartName="/word/document.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml"/>'
        '</Types>'
    )
    rels = (
        '<?xml version="1.0"?>'
        '<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
        '<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="word/document.xml"/>'
        '</Relationships>'
    )
    with zipfile.ZipFile(path, 'w', zipfile.ZIP_DEFLATED) as z:
        z.writestr('[Content_Types].xml', content_types)
        z.writestr('_rels/.rels', rels)
        z.writestr('word/document.xml', document)

def write_xlsx(path):
    shared = (
        '<?xml version="1.0"?>'
        '<sst xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" count="3" uniqueCount="3">'
        '<si><t>Name</t></si>'
        '<si><t>Alice</t></si>'
        '<si><t>Bob</t></si>'
        '</sst>'
    )
    workbook = (
        '<?xml version="1.0"?>'
        '<workbook xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" '
        'xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships">'
        '<sheets><sheet name="People" sheetId="1" r:id="rId1"/></sheets>'
        '</workbook>'
    )
    wb_rels = (
        '<?xml version="1.0"?>'
        '<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
        '<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet1.xml"/>'
        '</Relationships>'
    )
    sheet = (
        '<?xml version="1.0"?>'
        '<worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">'
        '<sheetData>'
        '<row r="1"><c r="A1" t="s"><v>0</v></c><c r="B1" t="s"><v>1</v></c></row>'
        '<row r="2"><c r="A2" t="s"><v>2</v></c><c r="B2" t="s"><v>2</v></c></row>'
        '<row r="3"><c r="A3"><v>42</v></c><c r="B3" t="b"><v>1</v></c></row>'
        '</sheetData></worksheet>'
    )
    content_types = (
        '<?xml version="1.0"?>'
        '<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">'
        '<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>'
        '<Default Extension="xml" ContentType="application/xml"/>'
        '<Override PartName="/xl/workbook.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml"/>'
        '<Override PartName="/xl/worksheets/sheet1.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml"/>'
        '<Override PartName="/xl/sharedStrings.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sharedStrings+xml"/>'
        '</Types>'
    )
    root_rels = (
        '<?xml version="1.0"?>'
        '<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
        '<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="xl/workbook.xml"/>'
        '</Relationships>'
    )
    with zipfile.ZipFile(path, 'w', zipfile.ZIP_DEFLATED) as z:
        z.writestr('[Content_Types].xml', content_types)
        z.writestr('_rels/.rels', root_rels)
        z.writestr('xl/workbook.xml', workbook)
        z.writestr('xl/_rels/workbook.xml.rels', wb_rels)
        z.writestr('xl/sharedStrings.xml', shared)
        z.writestr('xl/worksheets/sheet1.xml', sheet)

def main():
    docx = os.path.join(HERE, 'sample.docx')
    xlsx = os.path.join(HERE, 'sample.xlsx')
    write_docx(docx)
    write_xlsx(xlsx)
    sys.path.insert(0, '/home/wubu/hermes-agent-dev')
    from tools.read_extract import extract_document_text
    with open(os.path.join(HERE, 'sample.docx.ref.txt'), 'w') as f:
        f.write(extract_document_text(docx))
    with open(os.path.join(HERE, 'sample.xlsx.ref.txt'), 'w') as f:
        f.write(extract_document_text(xlsx))
    print('fixtures + python reference written')

if __name__ == '__main__':
    main()
