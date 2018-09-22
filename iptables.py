#!/usr/bin/python3

import os
import subprocess
import sys


class Config(dict):
    def __getattr__(self, key):
        return self[key]


def parse_config(fname):
    config_str = open(fname).read()
    config = Config()
    exec(config_str, config)
    return config


def prepare_args(action, config, ipv6):
    if action == 'add':
        action_flag = '-A'
    elif action == 'del':
        action_flag = '-D'

    host = config.backend_ipv6 if ipv6 else config.backend_ip

    return [
        '-t', 'nat',
        action_flag, 'PREROUTING',
        '-p', config.protocol,
        '--dport', str(config.port), '-j', 'DNAT',
        '--to-destination', f'{host}:{config.backend_port}',
        '-m', 'comment', '--comment', f'portcullis_{config.name}',
    ]


def exec_cmd(args, ipv6):
    prog = '/sbin/ip6tables' if ipv6 else '/sbin/iptables'
    cmd = [prog] + args
    if '-D' in cmd:
        while subprocess.call(cmd) == 0:
            pass
    else:
        subprocess.call(cmd)


def main():
    if len(sys.argv) < 3:
        print('Usage: {} add/del config.py'.format(sys.argv[0]))
        return

    action = sys.argv[1]

    if action not in ('add', 'del'):
        raise ValueError('Unknown action')

    if os.getuid() != 0:
        raise RuntimeError('You must be root to change IPTables')

    config = parse_config(sys.argv[2])

    if not config.backend_ip:  # IPv4
        args = prepare_args(action, config, ipv6=False)
        exec_cmd(args=args, ipv6=False)

    if config.backend_ipv6:
        args = prepare_args(action, config, ipv6=True)
        exec_cmd(args=args, ipv6=True)


if __name__ == '__main__':
    main()
