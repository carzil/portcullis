#!/usr/bin/env python

import attr
import json
from flask import Flask, request, send_file

@attr.s(auto_attribs=True, hash=True)
class Service(object):
    name: str
    config: str
    handler: str

app = Flask(__name__)

tmp = {
    'qwewqe',
    'dsad',
    'dsaczxfqe',
}
services = {
    i: Service(i, 'ololol', 'dsadas') for i in tmp
}


@app.route('/')
@app.route('/dist/build.js')
@app.route('/dist/build.js.map')
def serveStaticFile():
    files = {
        '/': 'index.html',
        '/dist/build.js': 'build.js',
        '/dist/build.js.map': 'build.js.map',
    }
    fname = files.get(request.path)
    assert fname
    return send_file('dist/' + fname)


@app.route('/api/services')
def listServices():
    return json.dumps(sorted(services.keys()))


@app.route('/api/service')
def getServiceInfo():
    s = request.args.get('name')
    assert s
    assert s in services
    return json.dumps(attr.asdict(services[s]))

if __name__ == '__main__':
    app.run(host='127.0.0.1', port=5000)
