#!/usr/bin/env python3
"""Creates an account on a Refresh server (LittleBigPlanet's levels, profiles and matchmaking) for an LBPPC player:
registers it through Refresh's web API and allows the PS3-style logins LBPPC's game makes. The online name must be
the same as the player's shadNet account (or local online name).

usage: python tools/refresh_account.py <online name> [--server http://host:10061] [--password pw] [--email address]
Without --password, a random one (for Refresh's website) is made and printed.
"""
import argparse
import hashlib
import json
import secrets
import urllib.error
import urllib.request


def call(url, body, token=None, method='POST'):
    request = urllib.request.Request(url, data=json.dumps(body).encode(), method=method,
                                     headers={'Content-Type': 'application/json', **({'Authorization': token} if token else {})})
    try:
        with urllib.request.urlopen(request) as reply:
            return json.load(reply)
    except urllib.error.HTTPError as error:
        return json.loads(error.read() or b'{}')


def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('name')
    parser.add_argument('--server', default='http://127.0.0.1:10061')
    parser.add_argument('--password')
    parser.add_argument('--email')
    args = parser.parse_args()
    password = args.password or secrets.token_urlsafe(12)
    email = args.email or f'{args.name.lower()}@lbppc.invalid'
    digest = hashlib.sha512(password.encode()).hexdigest()
    api = args.server.rstrip('/') + '/api/v3'
    reply = call(f'{api}/register', {'username': args.name, 'emailAddress': email, 'passwordSha512': digest})
    if not reply.get('success'):
        raise SystemExit(f'Not registered: {reply.get("error", reply)}')
    token = reply['data']['tokenData']
    settings = call(f'{api}/users/me', {'psnAuthenticationAllowed': True, 'rpcnAuthenticationAllowed': True},
                    token, 'PATCH')
    if not settings.get('success'):
        raise SystemExit(f'Registered, but game logins are not allowed yet: {settings.get("error", settings)}')
    print(f'Created {args.name} on {args.server} (game logins allowed)\npassword: {password}\nemail: {email}')


if __name__ == '__main__':
    main()
