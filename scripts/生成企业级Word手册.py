from __future__ import annotations

import re
from pathlib import Path

from docx import Document
from docx.enum.section import WD_SECTION
from docx.enum.table import WD_TABLE_ALIGNMENT, WD_CELL_VERTICAL_ALIGNMENT
from docx.enum.text import WD_ALIGN_PARAGRAPH, WD_BREAK
from docx.oxml import OxmlElement
from docx.oxml.ns import qn
from docx.shared import Cm, Inches, Pt, RGBColor


REPO_ROOT = Path(__file__).resolve().parents[1]
SOURCE = REPO_ROOT / "docs" / "企业级用户操作手册与产品说明书.md"
OUTPUT = REPO_ROOT / "docs" / "FaultPolyTrace企业级用户操作手册与产品说明书_V1.1.0.docx"


def set_east_asia_font(run, east_asia: str = "宋体", ascii_font: str = "Times New Roman") -> None:
    run.font.name = ascii_font
    r_pr = run._element.get_or_add_rPr()
    r_fonts = r_pr.rFonts
    if r_fonts is None:
        r_fonts = OxmlElement("w:rFonts")
        r_pr.append(r_fonts)
    r_fonts.set(qn("w:eastAsia"), east_asia)
    r_fonts.set(qn("w:ascii"), ascii_font)
    r_fonts.set(qn("w:hAnsi"), ascii_font)


def set_paragraph_spacing(paragraph, before=0, after=6, line=1.35) -> None:
    paragraph.paragraph_format.space_before = Pt(before)
    paragraph.paragraph_format.space_after = Pt(after)
    paragraph.paragraph_format.line_spacing = line


def set_cell_shading(cell, fill: str) -> None:
    tc_pr = cell._tc.get_or_add_tcPr()
    shd = tc_pr.find(qn("w:shd"))
    if shd is None:
        shd = OxmlElement("w:shd")
        tc_pr.append(shd)
    shd.set(qn("w:fill"), fill)


def set_cell_text(cell, text: str, bold: bool = False, color: str | None = None) -> None:
    cell.text = ""
    p = cell.paragraphs[0]
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER if len(text) <= 12 else WD_ALIGN_PARAGRAPH.LEFT
    set_paragraph_spacing(p, before=0, after=0, line=1.2)
    run = p.add_run(text)
    set_east_asia_font(run, "宋体", "Times New Roman")
    run.font.size = Pt(9)
    run.bold = bold
    if color:
        run.font.color.rgb = RGBColor.from_string(color)
    cell.vertical_alignment = WD_CELL_VERTICAL_ALIGNMENT.CENTER


def set_table_borders(table, color="A6B4C5", size="6") -> None:
    tbl_pr = table._tbl.tblPr
    borders = tbl_pr.first_child_found_in("w:tblBorders")
    if borders is None:
        borders = OxmlElement("w:tblBorders")
        tbl_pr.append(borders)
    for edge in ("top", "left", "bottom", "right", "insideH", "insideV"):
        tag = "w:" + edge
        element = borders.find(qn(tag))
        if element is None:
            element = OxmlElement(tag)
            borders.append(element)
        element.set(qn("w:val"), "single")
        element.set(qn("w:sz"), size)
        element.set(qn("w:space"), "0")
        element.set(qn("w:color"), color)


def set_table_width(table, widths_cm: list[float] | None = None) -> None:
    table.alignment = WD_TABLE_ALIGNMENT.CENTER
    table.autofit = True
    if not widths_cm:
        return
    for row in table.rows:
        for i, width in enumerate(widths_cm):
            if i < len(row.cells):
                row.cells[i].width = Cm(width)


def add_table_from_rows(doc: Document, rows: list[list[str]]) -> None:
    if not rows:
        return
    col_count = max(len(r) for r in rows)
    table = doc.add_table(rows=len(rows), cols=col_count)
    table.style = "FPT Table"
    set_table_borders(table)
    table.alignment = WD_TABLE_ALIGNMENT.CENTER
    for r_idx, row in enumerate(rows):
        for c_idx in range(col_count):
            text = row[c_idx].strip() if c_idx < len(row) else ""
            set_cell_text(table.cell(r_idx, c_idx), text, bold=(r_idx == 0), color=("FFFFFF" if r_idx == 0 else None))
            if r_idx == 0:
                set_cell_shading(table.cell(r_idx, c_idx), "1F4E79")
            elif r_idx % 2 == 0:
                set_cell_shading(table.cell(r_idx, c_idx), "F6F8FB")
    widths = suggest_widths(col_count)
    set_table_width(table, widths)
    doc.add_paragraph()


def suggest_widths(col_count: int) -> list[float]:
    if col_count == 2:
        return [4.0, 11.2]
    if col_count == 3:
        return [3.8, 5.4, 6.0]
    if col_count == 4:
        return [3.2, 3.2, 4.6, 4.2]
    if col_count == 5:
        return [2.6, 2.8, 3.2, 3.2, 3.4]
    if col_count == 6:
        return [2.2, 2.5, 2.4, 2.4, 2.4, 3.3]
    return [15.2 / max(col_count, 1)] * col_count


def parse_inline(paragraph, text: str) -> None:
    pattern = re.compile(r"(`[^`]+`|【[^】]+】|“[^”]+”)")
    pos = 0
    for match in pattern.finditer(text):
        if match.start() > pos:
            run = paragraph.add_run(text[pos:match.start()])
            set_east_asia_font(run)
        token = match.group(0)
        run = paragraph.add_run(token)
        if token.startswith("`"):
            set_east_asia_font(run, "Consolas", "Consolas")
            run.font.size = Pt(9.5)
            run.font.color.rgb = RGBColor(84, 84, 84)
        else:
            set_east_asia_font(run, "宋体", "Times New Roman")
            run.bold = True
            run.font.color.rgb = RGBColor(31, 78, 121)
        pos = match.end()
    if pos < len(text):
        run = paragraph.add_run(text[pos:])
        set_east_asia_font(run)


def add_code_block(doc: Document, code: str) -> None:
    p = doc.add_paragraph(style="FPT Code")
    for idx, line in enumerate(code.rstrip("\n").splitlines()):
        if idx:
            p.add_run().add_break()
        run = p.add_run(line)
        set_east_asia_font(run, "Consolas", "Consolas")
        run.font.size = Pt(8.5)
    set_paragraph_spacing(p, before=3, after=8, line=1.1)


def add_toc(paragraph) -> None:
    run = paragraph.add_run()
    fld_begin = OxmlElement("w:fldChar")
    fld_begin.set(qn("w:fldCharType"), "begin")
    instr = OxmlElement("w:instrText")
    instr.set(qn("xml:space"), "preserve")
    instr.text = 'TOC \\o "1-3" \\h \\z \\u'
    fld_sep = OxmlElement("w:fldChar")
    fld_sep.set(qn("w:fldCharType"), "separate")
    fld_end = OxmlElement("w:fldChar")
    fld_end.set(qn("w:fldCharType"), "end")
    run._r.append(fld_begin)
    run._r.append(instr)
    run._r.append(fld_sep)
    placeholder = OxmlElement("w:t")
    placeholder.text = "目录将在 Word 中自动更新。打开文档后右键目录并选择“更新域”。"
    run._r.append(placeholder)
    run._r.append(fld_end)


def extract_headings(markdown: str, max_level: int = 3) -> list[tuple[int, str]]:
    headings: list[tuple[int, str]] = []
    for line in markdown.splitlines():
        match = re.match(r"^(#{1,6})\s+(.*)$", line.rstrip())
        if not match:
            continue
        level = len(match.group(1))
        if level <= max_level:
            text = re.sub(r"`([^`]+)`", r"\1", match.group(2).strip())
            headings.append((level, text))
    return headings


def add_static_toc(doc: Document, headings: list[tuple[int, str]]) -> None:
    intro = doc.add_paragraph()
    intro.paragraph_format.first_line_indent = None
    intro.alignment = WD_ALIGN_PARAGRAPH.LEFT
    set_paragraph_spacing(intro, before=0, after=8, line=1.25)
    run = intro.add_run("以下为静态可搜索目录。Word 自动目录域保留在本页下方，打开文档后可右键选择“更新域”生成带页码目录。")
    set_east_asia_font(run, "宋体", "Times New Roman")
    run.font.size = Pt(9.5)
    run.font.color.rgb = RGBColor(89, 89, 89)

    for level, text in headings:
        p = doc.add_paragraph()
        p.paragraph_format.first_line_indent = None
        p.paragraph_format.left_indent = Cm(0.0 if level == 1 else 0.55 if level == 2 else 1.1)
        p.paragraph_format.space_before = Pt(0)
        p.paragraph_format.space_after = Pt(2)
        p.paragraph_format.line_spacing = 1.05
        run = p.add_run(text)
        set_east_asia_font(run, "宋体", "Times New Roman")
        run.font.size = Pt(9.5 if level == 1 else 9)
        if level == 1:
            run.bold = True
            run.font.color.rgb = RGBColor(31, 78, 121)


def apply_styles(doc: Document) -> None:
    section = doc.sections[0]
    section.top_margin = Cm(2.6)
    section.bottom_margin = Cm(2.4)
    section.left_margin = Cm(2.8)
    section.right_margin = Cm(2.6)
    section.header_distance = Cm(1.3)
    section.footer_distance = Cm(1.3)

    normal = doc.styles["Normal"]
    normal.font.name = "Times New Roman"
    normal.font.size = Pt(10.5)
    normal._element.rPr.rFonts.set(qn("w:eastAsia"), "宋体")
    normal.paragraph_format.first_line_indent = Cm(0.74)
    normal.paragraph_format.line_spacing = 1.35
    normal.paragraph_format.space_after = Pt(6)

    for name, size, color, before, after, east_font in [
        ("Heading 1", 16, "1F4E79", 18, 8, "黑体"),
        ("Heading 2", 13, "2F5597", 14, 6, "黑体"),
        ("Heading 3", 11.5, "1F4E79", 10, 4, "黑体"),
    ]:
        style = doc.styles[name]
        style.font.name = "Times New Roman"
        style.font.size = Pt(size)
        style.font.bold = True
        style.font.color.rgb = RGBColor.from_string(color)
        style._element.rPr.rFonts.set(qn("w:eastAsia"), east_font)
        style.paragraph_format.first_line_indent = None
        style.paragraph_format.space_before = Pt(before)
        style.paragraph_format.space_after = Pt(after)
        style.paragraph_format.keep_with_next = True

    if "FPT Code" not in [s.name for s in doc.styles]:
        code = doc.styles.add_style("FPT Code", 1)
        code.font.name = "Consolas"
        code.font.size = Pt(8.5)
        code._element.rPr.rFonts.set(qn("w:eastAsia"), "Consolas")
        code.paragraph_format.left_indent = Cm(0.3)
        code.paragraph_format.right_indent = Cm(0.3)
        code.paragraph_format.first_line_indent = None
        code.paragraph_format.space_before = Pt(3)
        code.paragraph_format.space_after = Pt(8)

    if "FPT Table" not in [s.name for s in doc.styles]:
        table_style = doc.styles.add_style("FPT Table", 3)
        table_style.font.name = "Times New Roman"
        table_style.font.size = Pt(9)
        table_style._element.rPr.rFonts.set(qn("w:eastAsia"), "宋体")


def add_cover(doc: Document) -> None:
    title = doc.add_paragraph()
    title.alignment = WD_ALIGN_PARAGRAPH.CENTER
    title.paragraph_format.space_before = Pt(95)
    title.paragraph_format.space_after = Pt(16)
    run = title.add_run("FaultPolyTrace 断层多边形自动追踪系统")
    set_east_asia_font(run, "黑体", "Times New Roman")
    run.font.size = Pt(24)
    run.bold = True
    run.font.color.rgb = RGBColor(31, 78, 121)

    sub = doc.add_paragraph()
    sub.alignment = WD_ALIGN_PARAGRAPH.CENTER
    sub.paragraph_format.space_after = Pt(30)
    run = sub.add_run("企业级用户操作手册 / 产品说明书")
    set_east_asia_font(run, "黑体", "Times New Roman")
    run.font.size = Pt(18)
    run.bold = True

    meta = [
        ["文档版本", "V1.1.0"],
        ["产品版本", "FaultPolyTrace V1.1.0"],
        ["发布日期", "2026-07-26"],
        ["产品类型", "Windows 桌面软件系统 / GeoEast 集成工具 / 本地部署软件"],
        ["适用对象", "构造解释人员、地球物理解释人员、客户管理员、系统管理员、运维人员、验收人员、竞赛评审人员"],
        ["保密等级", "内部资料 / 项目交付资料"],
        ["交付单位", "FaultPolyTrace 项目组"],
    ]
    table = doc.add_table(rows=len(meta), cols=2)
    table.style = "FPT Table"
    set_table_borders(table, "A6B4C5")
    for i, row in enumerate(meta):
        set_cell_text(table.cell(i, 0), row[0], bold=True)
        set_cell_text(table.cell(i, 1), row[1])
        set_cell_shading(table.cell(i, 0), "E8EEF5")
    set_table_width(table, [3.6, 11.6])

    p = doc.add_paragraph()
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    p.paragraph_format.space_before = Pt(40)
    r = p.add_run("【Logo 占位：FaultPolyTrace 产品标识】")
    set_east_asia_font(r, "黑体", "Times New Roman")
    r.font.size = Pt(11)
    r.font.color.rgb = RGBColor(84, 84, 84)
    doc.add_page_break()


def add_header_footer(doc: Document) -> None:
    section = doc.sections[0]
    header = section.header.paragraphs[0]
    header.text = ""
    header.alignment = WD_ALIGN_PARAGRAPH.LEFT
    r = header.add_run("FaultPolyTrace 断层多边形自动追踪系统 | 企业级用户操作手册")
    set_east_asia_font(r, "宋体", "Times New Roman")
    r.font.size = Pt(9)
    r.font.color.rgb = RGBColor(89, 89, 89)

    footer = section.footer.paragraphs[0]
    footer.text = ""
    footer.alignment = WD_ALIGN_PARAGRAPH.CENTER
    r = footer.add_run("内部资料  |  第 ")
    set_east_asia_font(r, "宋体", "Times New Roman")
    add_page_number(footer)
    r = footer.add_run(" 页")
    set_east_asia_font(r, "宋体", "Times New Roman")


def add_page_number(paragraph) -> None:
    run = paragraph.add_run()
    fld_begin = OxmlElement("w:fldChar")
    fld_begin.set(qn("w:fldCharType"), "begin")
    instr = OxmlElement("w:instrText")
    instr.set(qn("xml:space"), "preserve")
    instr.text = "PAGE"
    fld_end = OxmlElement("w:fldChar")
    fld_end.set(qn("w:fldCharType"), "end")
    run._r.append(fld_begin)
    run._r.append(instr)
    run._r.append(fld_end)


def parse_markdown(doc: Document, markdown: str) -> None:
    lines = markdown.splitlines()
    i = 0
    in_code = False
    code_lines: list[str] = []
    table_rows: list[list[str]] = []
    skip_title_block = True

    while i < len(lines):
        line = lines[i].rstrip()

        if line.startswith("```"):
            if in_code:
                add_code_block(doc, "\n".join(code_lines))
                code_lines = []
                in_code = False
            else:
                flush_table(doc, table_rows)
                table_rows.clear()
                in_code = True
            i += 1
            continue

        if in_code:
            code_lines.append(line)
            i += 1
            continue

        if skip_title_block:
            if line.startswith("## 0. "):
                skip_title_block = False
            else:
                i += 1
                continue

        if line.strip().startswith("|") and line.strip().endswith("|"):
            cells = [c.strip() for c in line.strip().strip("|").split("|")]
            if all(re.fullmatch(r":?-{3,}:?", c) for c in cells):
                i += 1
                continue
            table_rows.append(cells)
            i += 1
            continue
        else:
            flush_table(doc, table_rows)
            table_rows.clear()

        if not line.strip():
            i += 1
            continue

        heading = re.match(r"^(#{1,6})\s+(.*)$", line)
        if heading:
            level = len(heading.group(1))
            text = heading.group(2).strip()
            if level <= 3:
                p = doc.add_paragraph(style=f"Heading {level}")
            else:
                p = doc.add_paragraph(style="Heading 3")
            p.paragraph_format.first_line_indent = None
            parse_inline(p, text)
            i += 1
            continue

        bullet = re.match(r"^\s*-\s+(.*)$", line)
        numbered = re.match(r"^\s*\d+\.\s+(.*)$", line)
        if bullet:
            p = doc.add_paragraph(style="List Bullet")
            p.paragraph_format.first_line_indent = None
            set_paragraph_spacing(p, after=3, line=1.25)
            parse_inline(p, bullet.group(1).strip())
        elif numbered:
            p = doc.add_paragraph(style="List Number")
            p.paragraph_format.first_line_indent = None
            set_paragraph_spacing(p, after=3, line=1.25)
            parse_inline(p, numbered.group(1).strip())
        else:
            p = doc.add_paragraph()
            p.alignment = WD_ALIGN_PARAGRAPH.JUSTIFY
            set_paragraph_spacing(p, after=6, line=1.35)
            parse_inline(p, line.strip())
        i += 1

    flush_table(doc, table_rows)
    if code_lines:
        add_code_block(doc, "\n".join(code_lines))


def flush_table(doc: Document, rows: list[list[str]]) -> None:
    if rows:
        add_table_from_rows(doc, rows)


def build_docx() -> None:
    markdown = SOURCE.read_text(encoding="utf-8")
    doc = Document()
    apply_styles(doc)
    add_cover(doc)
    add_header_footer(doc)

    toc_title = doc.add_paragraph(style="Heading 1")
    toc_title.paragraph_format.first_line_indent = None
    parse_inline(toc_title, "目录")
    add_static_toc(doc, extract_headings(markdown, 2))
    field_label = doc.add_paragraph()
    field_label.paragraph_format.first_line_indent = None
    set_paragraph_spacing(field_label, before=10, after=4, line=1.15)
    label_run = field_label.add_run("Word 自动目录域")
    set_east_asia_font(label_run, "黑体", "Times New Roman")
    label_run.bold = True
    label_run.font.color.rgb = RGBColor(31, 78, 121)
    toc = doc.add_paragraph()
    toc.paragraph_format.first_line_indent = None
    add_toc(toc)
    doc.add_page_break()

    parse_markdown(doc, markdown)

    doc.core_properties.title = "FaultPolyTrace 断层多边形自动追踪系统 企业级用户操作手册 / 产品说明书"
    doc.core_properties.subject = "GeoEast 集成、断层多边形自动追踪、用户操作手册、产品说明书"
    doc.core_properties.author = "FaultPolyTrace 项目组"
    doc.core_properties.comments = "中文企业级交付文档，可搜索 Word 版。"
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    doc.save(OUTPUT)
    print(OUTPUT)


if __name__ == "__main__":
    build_docx()
