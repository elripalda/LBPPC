#!/usr/bin/env python3
"""Creates an account on a shadNet server (what a player signs in to for friends and multiplayer), the way the
game protocol's Create command does: an online id (3 to 16 letters, digits, - or _), a password and an email.
The launcher's Create account makes this one and the Refresh one together.

usage: python tools/shadnet_account.py <online id> [--server host:port] [--password pw] [--email address]
Without --password, a random one is made and printed.
"""
import argparse
from pathlib import Path
import secrets
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / 'scripts'))
from online_accounts import AccountError, create_shadnet_account, placeholder_email  # noqa: E402


def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('npid')
    parser.add_argument('--server', default='127.0.0.1:31313')
    parser.add_argument('--password')
    parser.add_argument('--email')
    args = parser.parse_args()
    password = args.password or secrets.token_urlsafe(12)
    email = args.email or placeholder_email(args.npid)
    try:
        create_shadnet_account(args.server, args.npid, password, email)
    except AccountError as error:
        raise SystemExit(f'Account not created: {error}')
    print(f'Created {args.npid} on {args.server}\npassword: {password}\nemail: {email}')


if __name__ == '__main__':
    main()
