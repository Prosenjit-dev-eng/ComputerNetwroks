import os
import re
import base64
import subprocess
import shutil
import markdown

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
MD_PATH = os.path.join(SCRIPT_DIR, "CDMA_REPORT.md")
HTML_PATH = os.path.join(SCRIPT_DIR, "CDMA_REPORT.html")
PDF_PATH = os.path.join(SCRIPT_DIR, "CDMA_REPORT.pdf")
CN4_PDF_PATH = os.path.join(SCRIPT_DIR, "CN-4.pdf")

with open(MD_PATH, "r", encoding="utf-8") as f:
    md_content = f.read()

# 1. Clean math syntax to beautiful HTML/Unicode
def format_math(text):
    # LaTeX blocks and equations
    text = re.sub(r'\$\$C_{\\text\{composite\}}\[t\]\s*=\s*\\sum_\{i=0\}\^\{n-1\}\s*S_i\[t\]\$\$',
                  r'<div class="math-eq"><i>C</i><sub>composite</sub>[<i>t</i>] = &sum;<sub><i>i</i>=0</sub><sup><i>n</i>&minus;1</sup> <i>S<sub>i</sub></i>[<i>t</i>]</div>', text)
    text = re.sub(r'\$\$G_p\s*=\s*10\s*\\log_\{10\}\(N\)\s*\\text\{\s*dB\}\$\$',
                  r'<div class="math-eq"><i>G<sub>p</sub></i> = 10 log<sub>10</sub>(<i>N</i>) dB</div>', text)
    text = re.sub(r'\$\$P_j\s*=\s*C_{\\text\{composite\}}\s*\\cdot\s*C_j[^$]+\$\$',
                  r'<div class="math-eq"><i>P<sub>j</sub></i> = <i>C</i><sub>composite</sub> &middot; <i>C<sub>j</sub></i> = <i>d<sub>j</sub></i>(<i>C<sub>j</sub></i> &middot; <i>C<sub>j</sub></i>) + &sum;<sub><i>i</i>&ne;<i>j</i></sub> <i>d<sub>i</sub></i>(<i>C<sub>i</sub></i> &middot; <i>C<sub>j</sub></i>) = <i>N</i> &middot; <i>d<sub>j</sub></i> &rArr; <i>d<sub>j</sub></i> = <i>P<sub>j</sub></i> / <i>N</i></div>', text)
    text = re.sub(r'\$\$P_j\s*=\s*d_j\s*\\cdot\s*N[^$]+\$\$',
                  r'<div class="math-eq"><i>P<sub>j</sub></i> = <i>d<sub>j</sub></i> &middot; <i>N</i> + 0 = <i>N</i> &middot; <i>d<sub>j</sub></i> &rArr; <i>d<sub>j</sub></i> = <i>P<sub>j</sub></i> / <i>N</i></div>', text)
    text = re.sub(r'\$\$\\hat\{b\}\s*=\s*\\begin\{cases\}[^$]+\\end\{cases\}\$\$',
                  r'<div class="math-eq">b&#770; = <b>1</b> if <i>P<sub>j</sub></i> &gt; 0, &nbsp; <b>0</b> if <i>P<sub>j</sub></i> &lt; 0</div>', text)
    text = re.sub(r'\$\$\\text\{SNR\}_\{[^}]+\}\s*=\s*10\^[^$]+\$\$',
                  r'<div class="math-eq">SNR<sub>linear</sub> = 10<sup>SNR<sub>dB</sub> / 10</sup>, &nbsp; &sigma;<sup>2</sup> = <i>P</i><sub>signal</sub> / SNR<sub>linear</sub></div>', text)
    text = re.sub(r'\$\$H_\{2k\}\s*=\s*\\begin\{bmatrix\}[^$]+\\end\{bmatrix\}\$\$',
                  r'<div class="math-eq"><i>H</i><sub>2<i>k</i></sub> = [ [<i>H<sub>k</sub></i>, <i>H<sub>k</sub></i>], [<i>H<sub>k</sub></i>, &minus;<i>H<sub>k</sub></i>] ]</div>', text)

    # Inline math symbols
    text = re.sub(r'\$G_p\s*=\s*10\s*\\log_\{10\}\(N\)\s*\\text\{\s*dB\}\$', r'<i>G<sub>p</sub></i> = 10 log<sub>10</sub>(<i>N</i>) dB', text)
    text = re.sub(r'\$G_p\$', r'<i>G<sub>p</sub></i>', text)
    text = re.sub(r'\$C_i\s*\\cdot\s*C_j\s*=\s*0\s*\\quad\s*\(\\forall\s*i\s*\\ne\s*j\)\$', r'<i>C<sub>i</sub></i> &middot; <i>C<sub>j</sub></i> = 0 (&forall; <i>i</i> &ne; <i>j</i>)', text)
    text = re.sub(r'\$C_i\s*\\cdot\s*C_j\s*=\s*0\s*\(?\\forall\s*i\s*\\ne\s*j\)?\$', r'<i>C<sub>i</sub></i> &middot; <i>C<sub>j</sub></i> = 0 (&forall; <i>i</i> &ne; <i>j</i>)', text)
    text = re.sub(r'\$C_i\s*\\cdot\s*C_i\s*=\s*N\$', r'<i>C<sub>i</sub></i> &middot; <i>C<sub>i</sub></i> = <i>N</i>', text)
    text = re.sub(r'\$C_i\s*\\cdot\s*C_j\s*=\s*0\$', r'<i>C<sub>i</sub></i> &middot; <i>C<sub>j</sub></i> = 0', text)
    text = re.sub(r'\$C_i\$', r'<i>C<sub>i</sub></i>', text)
    text = re.sub(r'\$C_j\$', r'<i>C<sub>j</sub></i>', text)
    text = re.sub(r'\$C_\{comp\}\$', r'<i>C</i><sub>comp</sub>', text)
    text = re.sub(r'\$C_{\\text\{composite\}}\$', r'<i>C</i><sub>composite</sub>', text)
    text = re.sub(r'\$S_i\[b\s*\\cdot\s*N\s*\+\s*k\]\s*=\s*d_i\[b\]\s*\\times\s*C_i\[k\][^$]*\$', r'<i>S<sub>i</sub></i>[<i>b</i>&middot;<i>N</i> + <i>k</i>] = <i>d<sub>i</sub></i>[<i>b</i>] &times; <i>C<sub>i</sub></i>[<i>k</i>]', text)
    text = re.sub(r'\$d_i\s*\\in\s*\\\{\+1,\s*-1\\\}\$', r'<i>d<sub>i</sub></i> &isin; {+1, &minus;1}', text)
    text = re.sub(r'\$d_i\$', r'<i>d<sub>i</sub></i>', text)
    text = re.sub(r'\$d_j\$', r'<i>d<sub>j</sub></i>', text)
    text = re.sub(r'\$d_3\s*=\s*0\$', r'<i>d</i><sub>3</sub> = 0', text)
    text = re.sub(r'\$P_j\s*>\s*0\$', r'<i>P<sub>j</sub></i> &gt; 0', text)
    text = re.sub(r'\$P_j\s*<\s*0\$', r'<i>P<sub>j</sub></i> &lt; 0', text)
    text = re.sub(r'\$P_j\$', r'<i>P<sub>j</sub></i>', text)
    text = re.sub(r'\$W_N\$', r'<i>W<sub>N</sub></i>', text)
    text = re.sub(r'\$W_2\$', r'<i>W</i><sub>2</sub>', text)
    text = re.sub(r'\$W_4\$', r'<i>W</i><sub>4</sub>', text)
    text = re.sub(r'\$W_8\$', r'<i>W</i><sub>8</sub>', text)
    text = re.sub(r'\$W_\{16\}\$', r'<i>W</i><sub>16</sub>', text)
    text = re.sub(r'\$H_1\s*=\s*\[1\]\$', r'<i>H</i><sub>1</sub> = [1]', text)
    text = re.sub(r'\$H_\{2k\}\$', r'<i>H</i><sub>2<i>k</i></sub>', text)
    text = re.sub(r'\$N\s*=\s*2\^k\s*\\ge\s*n\$', r'<i>N</i> = 2<sup><i>k</i></sup> &ge; <i>n</i>', text)
    text = re.sub(r'\$N\s*=\s*2\^\{\\lceil\s*\\log_2\s*n\s*\\rceil\}\$', r'<i>N</i> = 2<sup>&lceil;log<sub>2</sub> <i>n</i>&rceil;</sup>', text)
    text = re.sub(r'\$N\s*=\s*(\d+)\$', r'<i>N</i> = \1', text)
    text = re.sub(r'\$n\s*=\s*(\d+)\$', r'<i>n</i> = \1', text)
    text = re.sub(r'\$n\s*\\le\s*N\$', r'<i>n</i> &le; <i>N</i>', text)
    text = re.sub(r'\$n\s*\\ge\s*(\d+)\$', r'<i>n</i> &ge; \1', text)
    text = re.sub(r'\$2\s*\\le\s*n\s*\\le\s*(\d+)\$', r'2 &le; <i>n</i> &le; \1', text)
    text = re.sub(r'\$\\Delta\s*t\s*=\s*0\$', r'&Delta;<i>t</i> = 0', text)
    text = re.sub(r'\$\\Delta\s*t\s*\\ne\s*0\$', r'&Delta;<i>t</i> &ne; 0', text)
    text = re.sub(r'\$\\mathcal\{N\}\(0,\s*\\sigma\^2\)\$', r'&Nu;(0, &sigma;<sup>2</sup>)', text)
    text = re.sub(r'\$([0-9.]+)\\text\{\s*dB\}\$', r'\1 dB', text)
    text = re.sub(r'\$([0-9.]+)\\text\{\s*ms\}\$', r'\1 ms', text)
    text = re.sub(r'\$([0-9.]+)\\text\{\s*V\}\$', r'\1 V', text)
    text = re.sub(r'\$0\\text\{\s*V\}\$', r'0 V', text)

    # General replacements
    text = re.sub(r'\\ge', r'&ge;', text)
    text = re.sub(r'\\le', r'&le;', text)
    text = re.sub(r'\\approx', r'&approx;', text)
    text = re.sub(r'\\ne', r'&ne;', text)
    text = re.sub(r'\\in', r'&isin;', text)
    text = re.sub(r'\\times', r'&times;', text)
    text = re.sub(r'\\cdot', r'&middot;', text)
    text = re.sub(r'\\forall', r'&forall;', text)
    text = re.sub(r'\\to', r'&rarr;', text)
    text = re.sub(r'\\implies', r'&rArr;', text)
    text = re.sub(r'\\text\{([^}]+)\}', r'\1', text)
    text = re.sub(r'\$N\$', r'<i>N</i>', text)
    text = re.sub(r'\$n\$', r'<i>n</i>', text)
    text = re.sub(r'\$k\$', r'<i>k</i>', text)
    text = re.sub(r'\$([0-9.]+)%?\$', r'\1', text)
    text = re.sub(r'\$([^$]+)\$', r'\1', text)
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

/* --- TITLE PAGE BOX (MATCHING USER GUIDELINES) --- */
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

/* --- INDEX / TOC PAGE --- */
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
    max-width: 95%;
    max-height: 380px;
    margin: 8px auto;
    display: block;
    page-break-inside: avoid;
    border: 1px solid #e0e0e0;
    padding: 6px;
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

.math-eq {
    background-color: #f8fafc;
    border: 1px solid #e2e8f0;
    padding: 5px 12px;
    margin: 6px auto;
    text-align: center;
    font-style: italic;
    font-size: 11pt;
    width: fit-content;
    page-break-inside: avoid;
}

blockquote {
    border-left: 3px solid #3b82f6;
    margin: 6px 0;
    padding: 4px 12px;
    background-color: #f8fafc;
    color: #334155;
    font-size: 11pt;
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
<title>Computer Networks Lab Report - Assignment 04</title>
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
    # Also create CN-4.pdf copy
    shutil.copyfile(PDF_PATH, CN4_PDF_PATH)
    print(f"SUCCESS: Copied to: {CN4_PDF_PATH}")
else:
    print("PDF generation failed:", ret.stderr, ret.stdout)
