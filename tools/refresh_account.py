#!/usr/bin/env python3
"""Creates an account on a Refresh server (LittleBigPlanet's levels, profiles and matchmaking) for an LBPPC player:
registers it through Refresh's web API and allows the PS3-style logins LBPPC's game makes. The online name must be
the same as the player's shadNet account (or local online name). The launcher's Create account makes both.

usage: python tools/refresh_account.py <online name> [--server http://host:10061] [--password pw] [--email address]
Without --password, a random one (for Refresh's website) is made and printed.
"""
import argparse
from pathlib import Path
import secrets
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / 'scripts'))
from online_accounts import AccountError, create_refresh_account, placeholder_email  # noqa: E402


def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('name')
    parser.add_argument('--server', default='http://127.0.0.1:10061')
    parser.add_argument('--password')
    parser.add_argument('--email')
    args = parser.parse_args()
    password = args.password or secrets.token_urlsafe(12)
    email = args.email or placeholder_email(args.name)
    try:
        create_refresh_account(args.server, args.name, password, email)
    except AccountError as error:
        raise SystemExit(f'Not registered: {error}')
    print(f'Created {args.name} on {args.server} (game logins allowed)\npassword: {password}\nemail: {email}')


if __name__ == '__main__':
    main()
