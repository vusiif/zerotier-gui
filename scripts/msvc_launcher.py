"""Normalize localized /showIncludes lines so Ninja tracks headers reliably."""
import subprocess
import sys

sys.stdout.reconfigure(encoding="utf-8")
result = subprocess.run(sys.argv[1:], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
try:
    output = result.stdout.decode("utf-8")
except UnicodeDecodeError:
    output = result.stdout.decode("mbcs", errors="replace")
for line in output.splitlines():
    if line.startswith("注意: 包含文件:"):
        line = "Note: including file:" + line[len("注意: 包含文件:"):]
    print(line)
sys.exit(result.returncode)
