import glob
import logging
import os
import subprocess
import tempfile

import attr


WORKDIR = os.path.join(os.environ.get('XDG_RUNTIME_DIR'), 'portcullis')


def atomic_write(fname, data):
    fp = tempfile.NamedTemporaryFile(prefix=WORKDIR, delete=False)
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

    def start(self):
        self._save_files()
        if not self.proxying:
            self._run_systemctl('start')
            self.proxying = True

    def stop(self):
        if self.proxying:
            self._run_systemctl('stop')
        self.proxying = False

    def delete(self):
        os.remove(f'{WORKDIR}/{self.name}.config.py')
        os.remove(f'{WORKDIR}/{self.name}.handler.py')

    def reload(self):
        # TODO: backup configs
        self._save_files()
        if self.proxying:
            self._run_systemctl('reload')
            # TODO: check for reload failed

    def is_alive(self):
        # checks exit code of systemctl status
        return not bool(self._run_systemctl('status'))

    def _save_files(self):
        if not os.path.isdir(WORKDIR):
            os.makedirs(WORKDIR)

        self.config = self.config.replace('$WORK_DIR', WORKDIR)
        self.handler = self.handler.replace('$WORK_DIR', WORKDIR)

        atomic_write(f'{WORKDIR}/{self.name}.config.py', self.config)
        atomic_write(f'{WORKDIR}/{self.name}.handler.py', self.handler)

    def _run_systemctl(self, cmd):
        return subprocess.call(
            ['systemctl', '--user', '--quiet', cmd, f'portcullis@{self.name}.service'],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        )


all = {}

configs = glob.glob(os.path.join(WORKDIR, '*.config.py'))
for fname in configs:
    try:
        fname = fname[fname.rindex('/') + 1:]
        name = fname[:fname.index('.')]
        config = open(f'{WORKDIR}/{name}.config.py').read()
        handler = open(f'{WORKDIR}/{name}.handler.py').read()
        service = Service(name, config, handler)
        logging.info('Loaded', service.name, 'is_active', service.proxying)
        all[name] = service
    except Exception:
        logging.exception(f'Exception on config {fname} load')
