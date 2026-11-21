# Writes the two bytes of one UTF-8 character ("ü") to each pipe, interleaved,
# with a pause between every write so each lands in its own read. A shared
# decoder would join stdout's lead byte with stderr's continuation byte.
import sys
import time

out = sys.stdout.buffer
err = sys.stderr.buffer
u = "ü".encode("utf-8")      # b"\xc3\xbc"
j = "日".encode("utf-8")     # three bytes

def put(stream, data):
    stream.write(data)
    stream.flush()
    time.sleep(0.15)

put(out, u[:1])
put(err, j[:1])
put(out, u[1:] + b"\n")
put(err, j[1:2])
put(err, j[2:] + b"\n")
