import struct
from portcullis.core import TcpHandle


def read_packed(handle, fmt):
    size = struct.calcsize(fmt)
    return struct.unpack(fmt, handle.read_exactly(size))


def write_packed(handle, fmt, *args, **kwargs):
    size = struct.calcsize(fmt)
    return handle.write_all(struct.pack(fmt, *args, **kwargs))
