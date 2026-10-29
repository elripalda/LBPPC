#!/usr/bin/env python3
"""Start the game: read its eboot, link it with its bundled modules, run bb-probe.

usage: python run.py <game folder> [--timeout seconds] [more bb-probe options]
The game folder is the player's own dump (eboot.bin, sce_module, sce_sys). Generated files go to out/,
saves to user/. The log goes to the console and out/last_run.log.
"""
import os
from pathlib import Path
import subprocess
import sys

HERE = Path(__file__).resolve().parent
OUT = HERE / 'out'
MSYS2_BIN = Path(r'C:\msys64\clang64\bin')  # development builds: SDL3 and the other CLANG64 DLLs


def script(name, *args):
    result = subprocess.run([sys.executable, str(HERE / 'scripts' / name), *map(str, args)], stdin=subprocess.DEVNULL)
    if result.returncode:
        sys.exit(f'{name} failed ({result.returncode})')


def main(argv):
    if not argv or argv[0].startswith('-'):
        sys.exit(__doc__)
    game = Path(argv[0]).resolve()
    if not (game / 'eboot.bin').is_file():
        sys.exit(f'No eboot.bin in {game}')
    probe = OUT / 'bb-probe.exe'
    if not probe.is_file():
        sys.exit('out/bb-probe.exe not found: build it with build.sh (MSYS2 CLANG64)')
    user = HERE / 'user'
    user.mkdir(exist_ok=True)
    script('prepare.py', game, '--out', OUT)
    script('link_modules.py', game, '--out', OUT)
    env = dict(os.environ)
    if MSYS2_BIN.is_dir():
        env['PATH'] = f'{MSYS2_BIN}{os.pathsep}{env.get("PATH", "")}'
    command = [str(probe), str(OUT / 'boot-linked.bin'), '--app0', str(game), '--user', str(user), *argv[1:]]
    with (OUT / 'last_run.log').open('wb') as log:
        process = subprocess.Popen(command, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT, env=env, cwd=HERE)
        for line in process.stdout:
            log.write(line)
            sys.stdout.buffer.write(line)
            sys.stdout.flush()
        return process.wait()


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
