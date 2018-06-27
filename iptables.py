#!/usr/bin/env python

import shlex
import subprocess
import sys
import os


def main():
    if len(sys.argv) < 3:
        print('Usage: {} add/del config.py'.format(sys.argv[0]))
        return

    action = sys.argv[1]

    if action not in ('add', 'del'):
        raise ValueError('Unknown action')

    if os.getuid() != 0:
        raise RuntimeError('You must be root to change IPTables')

    config_file = sys.argv[2]
    config_str = open(config_file).read()
    config = {}
    exec(config_str, config)

    name = config['name']
    port = config['port']
    backend_port = config['backend_port']
    backend_host = config['backend_host']

    action_flag = {
        'add': '-A',
        'del': '-D',
    }[action]

    cmd_str = (
        f'/sbin/iptables -t nat {action_flag} PREROUTING -p tcp '
        f'--dport {port} -j DNAT --to-destination {backend_host}:{backend_port} '
        f'-m comment --comment "Portcullis port forward for service {name}"'
    )
    cmd = shlex.split(cmd_str)

    subprocess.check_call(cmd)

    if action == 'del':
        while True:
            try:
                subprocess.check_call(cmd)
            except subprocess.CalledProcessError:
                # probably all rules deleted, if any other exists
                break


if __name__ == '__main__':
    main()
