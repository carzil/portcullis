from portcullis.core import TcpHandle, resolve_v4
from portcullis.http import HttpHandle


backend_addr = resolve_v4("tcp://localhost:80")


def handler(ctx, clt):
    client = HttpHandle(clt)
    backend = HttpHandle(TcpHandle.connect(ctx, backend_addr))

    request = client.read_request()
    backend.write_request(request)
    client.transfer_body(backend, request)

    ua = request.headers["User-Agent"]
    if ua is not None:
        request.headers["User-Agent"] = "portcullis"

    response = backend.read_response()
    response.headers["Server"] = "portcullis"

    client.write_response(response)
    backend.transfer_body(client, response)

    # print(request.method, request.url, response.status, response.reason)
