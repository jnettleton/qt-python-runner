import sys
for i in (1, 2, 3):
    sys.stdout.write(f"out{i}\n")
    sys.stdout.flush()
    sys.stderr.write(f"err{i}\n")
    sys.stderr.flush()
