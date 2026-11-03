#!/usr/bin/env python3
"""Creates an account on a shadNet server (what a player signs in to for friends and multiplayer), the way the
game protocol's Create command does: an online id (3 to 16 letters, digits, - or _), a password and an email.

usage: python tools/shadnet_account.py <online id> [--server host:port] [--password pw] [--email address]
Without --password, a random one is made and printed.
"""
import argparse
import secrets
import socket
import struct

HEADER = 15  # type u8, command u16, total size u32, packet id u64 (little-endian)
REQUEST, REPLY, NOTIFICATION, SERVER_INFO = 0, 1, 2, 3
CREATE = 2
ERRORS = {0: 'ok', 1: 'malformed', 2: 'invalid', 3: 'invalid input (name, password or email)', 4: 'too soon',
          10: 'creation error', 11: 'that name is taken', 12: 'email provider not allowed', 13: 'email in use'}


def proto_string(field, text):
    data = text.encode('utf-8')
    return bytes([field << 3 | 2]) + varint(len(data)) + data


def varint(value):
    out = bytearray()
    while True:
        byte = value & 0x7f
        value >>= 7
        out.append(byte | (0x80 if value else 0))
        if not value:
            return bytes(out)


def read_packet(sock):
    header = b''
    while len(header) < HEADER:
        chunk = sock.recv(HEADER - len(header))
        if not chunk:
            raise ConnectionError('the server closed the connection')
        header += chunk
    kind, command, total, packet_id = struct.unpack('<BHIQ', header)
    body = b''
    while len(body) < total - HEADER:
        chunk = sock.recv(total - HEADER - len(body))
        if not chunk:
            raise ConnectionError('the server closed the connection')
        body += chunk
    return kind, command, packet_id, body


def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('npid')
    parser.add_argument('--server', default='127.0.0.1:31313')
    parser.add_argument('--password')
    parser.add_argument('--email')
    args = parser.parse_args()
    password = args.password or secrets.token_urlsafe(12)
    email = args.email or f'{args.npid.lower()}@shadps4.local'
    host, _, port = args.server.partition(':')
    request = proto_string(1, args.npid) + proto_string(2, password) + proto_string(4, email)
    payload = struct.pack('<I', len(request)) + request
    with socket.create_connection((host, int(port or 31313)), timeout=10) as sock:
        kind, *_ = read_packet(sock)  # the server's ServerInfo comes first
        if kind != SERVER_INFO:
            raise SystemExit(f'unexpected first packet type {kind}')
        sock.sendall(struct.pack('<BHIQ', REQUEST, CREATE, HEADER + len(payload), 1) + payload)
        while True:
            kind, command, _id, body = read_packet(sock)
            if kind == REPLY and command == CREATE:
                break
    error = body[0] if body else 1
    if error:
        raise SystemExit(f'Account not created: {ERRORS.get(error, error)}')
    print(f'Created {args.npid} on {args.server}\npassword: {password}\nemail: {email}')


if __name__ == '__main__':
    main()
