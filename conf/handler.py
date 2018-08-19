from portcullis.core import TcpHandle, resolve
from portcullis.helpers import read_packed, write_packed


backend_addr = resolve("tcp://localhost:8080")


def handler(ctx, client):
    backend = TcpHandle.create(ctx)
    backend.connect(backend_addr)
    print(client.transfer_all(backend))
