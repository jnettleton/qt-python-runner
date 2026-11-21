# POSIX: ignores SIGTERM, then spawns a grandchild that inherits the ignore
# (an ignored disposition survives exec), reports its pid, and hangs. Only a
# group-wide SIGKILL stops this tree.
import signal
import subprocess
import sys
import time

signal.signal(signal.SIGTERM, signal.SIG_IGN)
child = subprocess.Popen(
    [sys.executable, "-c", "import time; time.sleep(600)"],
    stdin=subprocess.DEVNULL,
    stdout=subprocess.DEVNULL,
    stderr=subprocess.DEVNULL,
)
print(f"grandchild {child.pid}", flush=True)
while True:
    time.sleep(0.1)
