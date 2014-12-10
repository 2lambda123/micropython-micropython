import io
import sys
if hasattr(sys, 'print_exception'):
    print_exception = sys.print_exception
else:
    import traceback
    print_exception = lambda e, f: traceback.print_exception(None, e, None, file=f)

try:
    1/0
except Exception as e:
    buf = io.StringIO()
    print_exception(e, buf)
    s = buf.getvalue()
    for l in s.split("\n"):
        # uPy and CPy tracebacks differ in that CPy prints a source line for
        # each traceback entry. In this case, we know that offending line
        # has 4-space indent, so filter it out.
        if not l.startswith("    "):
            print(l)
