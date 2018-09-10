#!/usr/bin/python3

import os
import subprocess
import sys


def modify_rule(is_ipv6, is_del, protocol, backend_host, backend_port, port, name):
    prog = '/sbin/ip6tables' if is_ipv6 else '/sbin/iptables'
    action_flag = '-D' if is_del else '-A'
    cmd = [prog, '-t', 'nat', action_flag, 'PREROUTING', '-p', protocol,
        '--dport', str(port), '-j', 'DNAT', '--to-destination', '{backend_host}:{backend_port}'
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
    config_str = open(config_file).read()
    config = {}
    exec(config_str, config)

    backend_port = config['backend_port']
    name = config['name']
    port = config['port']
    protocol = config['protocol']

    if protocol not in ('tcp', 'udp'):
        raise ValueError('Unknown protocol')  

    if 'backend_host' in config:
        modify_rule(False, action == 'del', protocol, config['backend_host'], backend_port, port, name)

    if 'backend_ipv6host' in config:
        modify_rule(True, action == 'del', protocol, config['backend_ipv6host'], backend_port, port, name)


if __name__ == '__main__':
    main()
