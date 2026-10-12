import os
import re
import base64
import subprocess
import markdown

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
MD_PATH = os.path.join(SCRIPT_DIR, "ERROR_DETECTION_REPORT.md")
HTML_PATH = os.path.join(SCRIPT_DIR, "ERROR_DETECTION_REPORT.html")
PDF_PATH = os.path.join(SCRIPT_DIR, "ERROR_DETECTION_REPORT.pdf")

with open(MD_PATH, "r", encoding="utf-8") as f:
    md_content = f.read()

# 1. Clean math syntax and typography
def format_math(text):
    text = text.replace('–', '-').replace('—', ' - ')
    text = re.sub(r'\$([0-9.]+)%?\$', r'\1', text)
    text = re.sub(r'\$([a-zA-Z])\$', r'<i>\1</i>', text)
    return text

md_content_formatted = format_math(md_content)

# 2. Embed images (PNG as base64, SVG inlined)
def embed_images(match):
    alt_text = match.group(1)
    img_rel_path = match.group(2)
    img_full_path = os.path.normpath(os.path.join(SCRIPT_DIR, img_rel_path))
    if os.path.exists(img_full_path):
        ext = os.path.splitext(img_full_path)[1].lower().replace(".", "")
        if ext == "svg":
            with open(img_full_path, "r", encoding="utf-8") as svg_f:
                svg_data = svg_f.read()
            return f'<div class="svg-flowchart-container">{svg_data}</div>'
        else:
            mime = f"image/{ext}" if ext != "jpg" else "image/jpeg"
            with open(img_full_path, "rb") as img_file:
                encoded = base64.b64encode(img_file.read()).decode("utf-8")
            return f'![{alt_text}](data:{mime};base64,{encoded})'
    return match.group(0)

md_content_embedded = re.sub(r'!\[(.*?)\]\((.*?)\)', embed_images, md_content_formatted)

html_body = markdown.markdown(
    md_content_embedded,
    extensions=['extra', 'codehilite', 'tables', 'nl2br']
)

css_styles = """
@page {
    size: A4;
    margin: 14mm 14mm 14mm 14mm;
}

body {
    font-family: 'Times New Roman', Times, serif;
    line-height: 1.35;
    color: #000000;
    font-size: 12pt;
    margin: 0;
    padding: 0;
}

.page-break {
    page-break-after: always;
    break-after: page;
}

/* --- TITLE PAGE BOX (MATCHING USER IMAGE 1) --- */
.title-container {
    max-width: 630px;
    margin: 40px auto 0 auto;
    font-family: 'Times New Roman', Times, serif;
}

.title-header-box {
    border: 2px solid #000000;
    text-align: center;
    padding: 6px 10px;
    font-size: 15pt;
    font-weight: bold;
    margin-bottom: 14px;
}

.title-content-box {
    border: 2px solid #000000;
    padding: 18px 22px;
    font-size: 12pt;
    line-height: 1.45;
}

.title-content-box p {
    margin: 6px 0;
    color: #000000;
}

.title-content-box b {
    font-weight: bold;
}

/* --- INDEX / TOC PAGE (MATCHING USER IMAGE 2) --- */
.index-container {
    max-width: 630px;
    margin: 40px auto 0 auto;
    border: 2px solid #000000;
    padding: 20px 26px;
    font-family: 'Times New Roman', Times, serif;
}

.index-table {
    width: 100%;
    border-collapse: collapse;
    font-size: 12pt;
    margin: 0 !important;
}

.index-table th {
    background: none !important;
    border: none !important;
    padding: 4px 0 10px 0;
    font-size: 12pt;
    font-weight: bold;
    color: #000000;
}

.index-table td {
    background: none !important;
    border: none !important;
    padding: 3.5px 0;
    color: #000000;
}

.index-topic {
    text-align: left;
    font-weight: bold;
}

.index-pageno {
    text-align: right;
    font-weight: bold;
}

.index-table td:last-child {
    text-align: right;
    font-weight: bold;
}

/* --- COMPACT HEADINGS WITH POINTS & BOLD --- */
h1 {
    font-family: 'Times New Roman', Times, serif;
    font-size: 16pt;
    font-weight: bold;
    color: #000000;
    text-align: center;
    margin-top: 10px;
    margin-bottom: 12px;
}

h2 {
    font-family: 'Times New Roman', Times, serif;
    font-size: 13.5pt;
    font-weight: bold;
    color: #000000;
    border-bottom: 1px solid #000000;
    padding-bottom: 3px;
    margin-top: 14px;
    margin-bottom: 6px;
    page-break-after: avoid;
}

h3 {
    font-family: 'Times New Roman', Times, serif;
    font-size: 12pt;
    font-weight: bold;
    color: #000000;
    margin-top: 10px;
    margin-bottom: 4px;
    page-break-after: avoid;
}

h4 {
    font-family: 'Times New Roman', Times, serif;
    font-size: 12pt;
    font-weight: bold;
    color: #000000;
    margin-top: 8px;
    margin-bottom: 3px;
    page-break-after: avoid;
}

p, ul, ol {
    margin-top: 3px;
    margin-bottom: 5px;
}

li {
    margin-bottom: 2px;
}

table:not(.index-table) {
    width: 100%;
    border-collapse: collapse;
    margin: 8px 0;
    font-size: 10.5pt;
    font-family: 'Times New Roman', Times, serif;
    page-break-inside: avoid;
}

table:not(.index-table) th, table:not(.index-table) td {
    border: 1px solid #000000;
    padding: 4.5px 7px;
    text-align: left;
}

table:not(.index-table) th {
    background-color: #f0f0f0;
    color: #000000;
    font-weight: bold;
}

table:not(.index-table) tr:nth-child(even) td {
    background-color: #fafafa;
}

pre {
    background-color: #fcfcfc;
    border: 1px solid #cccccc;
    border-radius: 4px;
    padding: 6px 10px;
    font-family: 'Consolas', 'Courier New', monospace;
    font-size: 9pt;
    line-height: 1.25;
    margin: 6px 0;
    overflow-x: auto;
    page-break-inside: avoid;
    white-space: pre-wrap;
    word-break: break-word;
}

code {
    font-family: 'Consolas', 'Courier New', monospace;
    background-color: #f2f2f2;
    padding: 1px 3px;
    border-radius: 2px;
    font-size: 9pt;
    color: #000000;
}

pre code {
    background: none;
    padding: 0;
    color: #000000;
}

img {
    max-width: 95%;
    max-height: 380px;
    height: auto;
    display: block;
    margin: 8px auto;
    border: 1px solid #e0e0e0;
    page-break-inside: avoid;
}

.svg-flowchart-container {
    max-width: 98%;
    max-height: 380px;
    margin: 8px auto;
    display: block;
    page-break-inside: avoid;
    border: 1px solid #e0e0e0;
    padding: 4px;
    background-color: #ffffff;
    text-align: center;
}

.svg-flowchart-container svg {
    display: block;
    margin: 0 auto;
    max-height: 365px;
    width: auto;
    max-width: 100%;
}

hr {
    border: none;
    border-top: 1px solid #cccccc;
    margin: 10px 0;
}
"""

html_full = f"""<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<title>Computer Networks Lab Report - Assignment 01</title>
<style>
{css_styles}
</style>
</head>
<body>
{html_body}
</body>
</html>
"""

with open(HTML_PATH, "w", encoding="utf-8") as f:
    f.write(html_full)

print(f"Generated HTML: {HTML_PATH}")

browser_candidates = [
    r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe",
    r"C:\Program Files\Microsoft\Edge\Application\msedge.exe",
    r"C:\Program Files\Google\Chrome\Application\chrome.exe"
]

browser_exe = None
for b in browser_candidates:
    if os.path.exists(b):
        browser_exe = b
        break

if not browser_exe:
    print("Error: Neither Edge nor Chrome found.")
    exit(1)

cmd = [
    browser_exe,
    "--headless=new",
    "--disable-gpu",
    "--no-sandbox",
    "--run-all-compositor-stages-before-draw",
    f"--print-to-pdf={PDF_PATH}",
    "--no-pdf-header-footer",
    f"file:///{HTML_PATH.replace(os.sep, '/')}"
]

ret = subprocess.run(cmd, capture_output=True, text=True)
if os.path.exists(PDF_PATH) and os.path.getsize(PDF_PATH) > 0:
    print(f"SUCCESS: PDF generated at: {PDF_PATH} ({os.path.getsize(PDF_PATH):,} bytes)")
else:
    print("PDF generation failed:", ret.stderr, ret.stdout)
