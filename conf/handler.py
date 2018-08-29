from portcullis.core import TcpHandle, resolve_v4
from portcullis.helpers import read_packed, write_packed


backend_addr = resolve_v4("tcp://localhost:8080")


def handler(ctx, client):
    backend = TcpHandle.connect(ctx, backend_addr)
    print(client.transfer_all(backend))
