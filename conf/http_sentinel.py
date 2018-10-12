import json
from base64 import b64decode
from portcullis.http.sentinel import *


def bad_request(request):
    return "SELECT" in request.url


def bad_response(response):
    return "bad_cookie" in response.cookies


# * Every section starts with default policy: Drop/Allow.
# * All conditions (they are called 'blocks') inside section are OR'ed, i.e. Drop/Allow occurs
#   if at least one block is satisfied.
# * All blocks are ordered in a such way that request blocks will be applied first.
# * And will be applied after all required parts of HTTP conversation are available


handler = Sentinel(
    ("/deny", Deny([
        # ban request if regexp presents in body
        RequestBodyContainsRegexp(r"SELECT|UNION|#/bin/bash"),

        # ban response if regexp presents in body, but accumulate whole body in memory
        # if memory limit exceeds while accumulating, response automatically will be discarded
        ResponseBodyContainsRegexp(r"flg{asdfasd}", threshold=150, streaming=False),

        # ban request if function returns True
        RequestIf(bad_request),

        # ban response if function returns True
        ResponseIf(bad_request)
    ])),

    ("/allow", Allow([
        # allow if header value matches regexp
        RequestHeaderMatches("User-Agent", r"python-requests/\d+.\d+"),

        # allow if header presents in request
        ResponseHasHeader("User-Agent2").

        # allow request if function returns True
        RequestIf(bad_request),

        # allow response if function returns True
        ResponseIf(bad_request),

        # allow request if functions returns True
        # function should accept body as bytes for first argument
        RequestBodyIf(lambda body: "admin" not in b64decode(body)),

        ResponseBodyIf(lambda body: "good_action" in json.loads(body))
    ])),

    # blocks can be assembled into single And section
    ("/composite", Allow([
        # allow if request contains both headers
        And(RequestHasHeader("User-Agent"), RequestHasHeader("User-Agent2")),

        And(RequestIf(bad_request), ResponseIf(bad_response))
    ]))
)
