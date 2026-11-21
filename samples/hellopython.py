"""Sample script for trying Python Runner by hand (GUI or pyrun). Stdlib only."""
import sys
import time

print("Hello from Python Runner!")
print(f"Python {sys.version.split()[0]} at {sys.executable}")
print("Unicode check: Grüße – 日本語 – ✓")
print("This line goes to stderr.", file=sys.stderr)

# Short sleeps so live streaming (and the Stop button) can be seen working.
for i in range(1, 7):
    time.sleep(0.5)
    print(f"tick {i}/6", flush=True)

print("Done.")
sys.exit(0)
