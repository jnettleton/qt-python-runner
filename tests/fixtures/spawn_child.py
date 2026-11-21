# Spawns a grandchild that sleeps for 10 minutes, reports its pid, then hangs.
import subprocess
import sys
import time

child = subprocess.Popen(
    [sys.executable, "-c", "import time; time.sleep(600)"],
    stdin=subprocess.DEVNULL,
    stdout=subprocess.DEVNULL,
    stderr=subprocess.DEVNULL,
)
print(f"grandchild {child.pid}", flush=True)
while True:
    time.sleep(0.1)
