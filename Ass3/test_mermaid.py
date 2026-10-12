import subprocess, os

html = """<!DOCTYPE html>
<html>
<head>
<script src="mermaid.min.js"></script>
<script>
window.onload = function() {
    mermaid.initialize({startOnLoad: true, theme: 'default'});
};
</script>
</head>
<body>
<h1>Mermaid Test</h1>
<div class="mermaid">
graph TD
    A[Start] --> B{Is Idle?}
    B -- Yes --> C[Transmit]
    B -- No --> D[Backoff]
</div>
</body>
</html>"""

with open("test_mermaid.html", "w", encoding="utf-8") as f:
    f.write(html)

cmd = [
    r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe",
    "--headless=new",
    "--disable-gpu",
    "--no-sandbox",
    "--virtual-time-budget=5000",
    "--run-all-compositor-stages-before-draw",
    "--print-to-pdf=test_mermaid.pdf",
    "--no-pdf-header-footer",
    f"file:///{os.path.abspath('test_mermaid.html').replace(os.sep, '/')}"
]
res = subprocess.run(cmd, capture_output=True, text=True)
print("PDF generated:", os.path.exists("test_mermaid.pdf"), os.path.getsize("test_mermaid.pdf"))
