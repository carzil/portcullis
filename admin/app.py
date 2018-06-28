#!/usr/bin/env python

import glob
import os
import subprocess
import tempfile

import attr
from flask import Flask, request, send_file, jsonify

PORTCULLIS_WORK_DIR = os.path.join(os.environ.get('XDG_RUNTIME_DIR'), 'portcullis')

def atomic_write(fname, data):
    fp = tempfile.NamedTemporaryFile(prefix=PORTCULLIS_WORK_DIR, delete=False)
    fp.write(data.encode('utf8'))
    fp.flush()
    os.rename(fp.name, fname)
    # no need to remove fp

@attr.s(auto_attribs=True, hash=True)
class Service(object):
    name: str
    config: str
    handler: str
    proxying: bool = False

    def __attrs_post_init__(self):
        self.proxying = self.is_alive()
        print(self.name, self.proxying)

    def start(self):
        self._save_files()
        if not self.proxying:
            self._run_systemctl('start')
            self.proxying = True

    def stop(self):
        if self.proxying:
            self._run_systemctl('stop')
        self.proxying = False

    def reload(self):
        self._save_files()
        if self.proxying:
            self._run_systemctl('reload')
            # TODO: check for reload failed

    def is_alive(self):
        # checks exit code of systemctl status
        return not bool(self._run_systemctl('status'))

    def _save_files(self):
        if not os.path.isdir(PORTCULLIS_WORK_DIR):
            os.makedirs(PORTCULLIS_WORK_DIR)

        self.config = self.config.replace('$WORK_DIR', PORTCULLIS_WORK_DIR)
        self.handler = self.handler.replace('$WORK_DIR', PORTCULLIS_WORK_DIR)

        atomic_write(f'{PORTCULLIS_WORK_DIR}/{self.name}.config.py', self.config)
        atomic_write(f'{PORTCULLIS_WORK_DIR}/{self.name}.handler.py', self.handler)

    def _run_systemctl(self, cmd):
        return subprocess.call([
            'systemctl', '--user', cmd, f'portcullis@{self.name}.service'
        ])


app = Flask(__name__)
services = {}


def on_load():
    global services
    configs = glob.glob(os.path.join(PORTCULLIS_WORK_DIR, '*.config.py'))
    for fname in configs:
        try:
            fname = fname[fname.rindex('/') + 1:]
            name = fname[:fname.index('.')]
            config = open(f'{PORTCULLIS_WORK_DIR}/{name}.config.py').read()
            handler = open(f'{PORTCULLIS_WORK_DIR}/{name}.handler.py').read()
            service = Service(name, config, handler)
            print(service)
            services[name] = service
        except Exception:
            import logging
            logging.exception('fuck')

on_load()


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
    assert fname
    return send_file('dist/' + fname)


@app.route('/api/services')
def services_list():
    return jsonify(sorted(services.keys()))


@app.route('/api/services/<name>', methods=['GET', 'POST', 'DELETE'])
def service_action(name):
    global services
    if request.method == 'GET':
        assert name in services
        return jsonify(attr.asdict(services[name]))
    elif request.method == 'POST':
        data = request.get_json()
        if name in services:
            if 'proxying' in data:
                if data['proxying']:
                    services[name].start()
                else:
                    services[name].stop()
            else:
                services[name].config = data['config']
                services[name].handler = data['handler']
                services[name].reload()
        else:
            serv = Service(name, data['config'], data['handler'])
            services[name] = serv
            serv.start()
        return 'ok'
    elif request.method == 'DELETE':
        assert name in services
        services[name].stop()
        del services[name]
        return 'ok'
    else:
        return ('Method not allowed', 405)

if __name__ == '__main__':
    app.run(host='127.0.0.1', port=5000)
