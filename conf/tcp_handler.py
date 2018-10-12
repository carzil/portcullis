from portcullis.core import TcpHandle, resolve_v4
from portcullis import re

db = re.compile_stream(
    ("abc", re.CASELESS),
    "a[b]{3,5}c",
)

matcher = re.StreamRegexpMatcher(db, lambda r, start, end: print(r, start, end))
matcher.scan("AB")
matcher.scan("C")
matcher.scan("ab")
matcher.scan("abbbbc")

def handler(ctx, client):
    pass
