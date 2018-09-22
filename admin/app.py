#!/usr/bin/env python

import attr
from flask import Flask, request, send_file, jsonify

import services

app = Flask(__name__)


@app.route('/')
@app.route('/dist/build.js')
@app.route('/dist/build.js.map')
def serve_static():
    files = {
        '/': 'index.html',
        '/dist/build.js': 'build.js',
        '/dist/build.js.map': 'build.js.map',
    }
    fname = files.get(request.path)
    return send_file('dist/' + fname)


def patch_service(name, data):
    if name in services.all:
        print('Patching', name, data)
        services.all[name].config = data['config']
        services.all[name].handler = data['handler']
        services.all[name].reload()
    else:
        print('Creating', name, data)
        serv = services.Service(name, data['config'], data['handler'])
        services.all[name] = serv
        serv.start()


@app.route('/api/services', methods=['GET', 'POST'])
def services_actions():
    if request.method == 'GET':
        data = {k: attr.asdict(v) for k, v in services.all.items()}
        return jsonify(data)
    elif request.method == 'POST':
        data = request.get_json()
        for name, config in data.items():
            patch_service(name, config)
        return 'ok'


@app.route('/api/service/<name>', methods=['GET', 'DELETE'])
def service_action(name):
    assert name in services.all
    if request.method == 'GET':
        return jsonify(attr.asdict(services.all[name]))
    elif request.method == 'DELETE':
        print('Removing', name)
        services.all[name].stop()
        services.all[name].delete()
        del services.all[name]
        return 'ok'


@app.route('/api/service/<name>/config', methods=['POST'])
def service_update(name):
    config = request.get_json()
    patch_service(name, config)
    return 'ok'


@app.route('/api/service/<name>/running', methods=['POST'])
def service_set_running(name):
    assert name in services.all
    is_running = request.get_json()['running']
    if is_running:
        print('Starting', name)
        services.all[name].start()
    else:
        print('Stopping', name)
        services.all[name].stop()
    return 'ok'


if __name__ == '__main__':
    app.run(host='127.0.0.1', port=5000)
