#!/usr/bin/python3

import json
import os
import subprocess
import sys


def modify_rule(is_ipv6, is_del, portcullis_host, portcullis_port, port, name):
    prog = '/sbin/ip6tables' if is_ipv6 else '/sbin/iptables'
    action_flag = '-D' if is_del else '-A'
    cmd = [prog, '-t', 'nat', action_flag, 'PREROUTING', '-p', 'tcp',
        '--dport', str(port), '-j', 'DNAT', '--to-destination', '{portcullis_host}:{portcullis_port}'
        '-m', 'comment', '--comment', 'Portcullis port forward for service {name}'
    ]

    if is_del:
        while subprocess.call(cmd) == 0:
            pass
    else:
        subprocess.check_call(cmd)


def main():
    if len(sys.argv) < 3:
        print('Usage: {} add/del config.json'.format(sys.argv[0]))
        return

    action = sys.argv[1]

    if action not in ('add', 'del'):
        raise ValueError('Unknown action')

    if os.getuid() != 0:
        raise RuntimeError('You must be root to change IPTables')

    config_file = sys.argv[2]
    config = json.load(open(config_file))

    for val in config['ports']:
        modify_rule(False, action == 'del', config['portcullis_host'], val['portcullis_port'], val['port'], config['name'])

    for val in config['ipv6ports']:
        modify_rule(False, action == 'del', config['portcullis_ipv6host'], val['portcullis_port'], val['port'], config['name'])


if __name__ == '__main__':
    main()
