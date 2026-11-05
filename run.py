#!/usr/bin/env python3
"""Start the game: read its eboot, link it with its bundled modules, run bb-probe.

usage: python run.py <game folder> [--timeout seconds] [more bb-probe options]
The game folder is the player's own dump (eboot.bin, sce_module, sce_sys). Generated files go to out/,
saves to user/. The log goes to the console and out/last_run.log.
"""
import json
import os
from pathlib import Path
import subprocess
import sys
import time

HERE = Path(__file__).resolve().parent
OUT = HERE / 'out'
MSYS2_BIN = Path(r'C:\msys64\clang64\bin')  # development builds: SDL3 and the other CLANG64 DLLs
RESTART_EXIT_CODE = 75  # the in-game menu's Apply and restart (probe.c runtime_restart)


def script(name, *args):
    # Packaged, sys.executable is LBPPC.exe (its Python frozen in), which runs scripts with --script.
    python = [sys.executable, '--script'] if getattr(sys, 'frozen', False) else [sys.executable]
    result = subprocess.run([*python, str(HERE / 'scripts' / name), *map(str, args)], stdin=subprocess.DEVNULL)
    if result.returncode:
        sys.exit(f'{name} failed ({result.returncode})')


def keep_previous_log(keep=10):
    # The last run's log moves to out/logs/run-<when it ended>.log, so a play session's log survives later starts.
    last = OUT / 'last_run.log'
    if not last.is_file():
        return
    logs = OUT / 'logs'
    logs.mkdir(exist_ok=True)
    when = time.strftime('%Y%m%d-%H%M%S', time.localtime(last.stat().st_mtime))
    try:
        last.replace(logs / f'run-{when}.log')
    except OSError:
        return
    for old in sorted(logs.glob('run-*.log'))[:-keep]:
        old.unlink(missing_ok=True)


def system_summary():
    """One line for the top of the log: the port's version and the PC it runs on (the GPU and its driver follow
    from the renderer), so a player's log says what it came from."""
    import platform
    cpu, ram = platform.processor() or '?', '?'
    if os.name == 'nt':
        import ctypes
        import winreg
        try:
            key = winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, r'HARDWARE\DESCRIPTION\System\CentralProcessor\0')
            cpu = winreg.QueryValueEx(key, 'ProcessorNameString')[0].strip()
        except OSError:
            pass

        class MemoryStatus(ctypes.Structure):
            _fields_ = [('length', ctypes.c_ulong), ('load', ctypes.c_ulong)] + \
                       [(name, ctypes.c_ulonglong) for name in ('total', 'avail', 'page_total', 'page_avail',
                                                                'virtual_total', 'virtual_avail', 'extended')]
        status = MemoryStatus(length=ctypes.sizeof(MemoryStatus))
        if ctypes.windll.kernel32.GlobalMemoryStatusEx(ctypes.byref(status)):
            ram = f'{status.total / 2**30:.0f} GB'
    version = os.environ.get('LBPPC_VERSION', 'development build')
    return (f'System: LBPPC {version}; {platform.system()} {platform.release()} ({platform.version()}); {cpu}, '
            f'{os.cpu_count()} threads; {ram} RAM\n')


def main(argv):
    if not argv or argv[0].startswith('-'):
        sys.exit(__doc__)
    game = Path(argv[0]).resolve()
    if not (game / 'eboot.bin').is_file():
        sys.exit(f'No eboot.bin in {game}')
    # Packaged releases keep the runtime in bin/; development builds in out/.
    probe = HERE / 'bin' / 'bb-probe.exe' if (HERE / 'bin' / 'bb-probe.exe').is_file() else OUT / 'bb-probe.exe'
    if not probe.is_file():
        sys.exit('out/bb-probe.exe not found: build it with build.sh (MSYS2 CLANG64)')
    # LBPPC_USER_DIR (tests): saves and settings somewhere other than user/.
    user = Path(os.environ['LBPPC_USER_DIR']) if os.environ.get('LBPPC_USER_DIR') else HERE / 'user'
    user.mkdir(exist_ok=True)
    script('prepare.py', game, '--out', OUT)
    # The game's own modules (LBP3's FMOD plugins) are linked too; sceKernelLoadStartModule hands them out.
    plugins = sorted(p.relative_to(game).as_posix() for p in game.glob('gamedata_orbis/spu/*.prx'))
    script('link_modules.py', game, '--out', OUT, '--modules', 'libc.prx', 'libSceFios2.prx', *plugins)
    script('content_profile.py', game, '--out', OUT)
    env = dict(os.environ)
    # Bloodborne's parallel draw preparation (the draw pipeline and its workers) corrupts memory under LBP3
    # (random crashes in Rasterizer::RetireSubmission and heap frees); off until that is fixed.
    env.setdefault('BB_DRAW_PIPE', '0')
    env.setdefault('BB_PREP_WORKERS', '0')
    # The executable is loaded below 4 GiB, as the PS4 does: LBP3's SPU job code keeps pointers to the
    # executable's globals in 32 bits (a job counter at 0x8012a4d00 was truncated, its jobs' output grew
    # without bound, and every increment corrupted host memory).
    env.setdefault('LBPPC_LOW_BASE', '0x400000')
    # LBP3's compute lighting binds its output image also as a written storage buffer; invalidating the
    # image for that write re-uploaded it from memory the GPU never wrote (black 3D scenes).
    env.setdefault('LBPPC_NO_BUFFER_IMAGE_INVALIDATE', '1')
    # LBP3 steps its world once per frame (30 per second), so 60 FPS comes from frame generation; its
    # camera, which frame generation needs, is recovered from the draws' matrices (lbp_camera.h).
    env.setdefault('LBPPC_LBP_CAMERA', '1')
    # Lower input latency: NVIDIA Reflex (AMD Anti-Lag 2 on AMD) with the 30 FPS limit kept before each game frame
    # instead of the game waiting on 60 Hz vblanks: from 84 to 29 ms between a frame's start and its present.
    env.setdefault('BB_LOW_LATENCY', 'on')
    env.setdefault('BB_VBLANK_HZ', '0')
    env.setdefault('BB_FPS_LIMIT', '30')
    # Beta: frame rate, pacing and where each frame's time went, every 5 s in the log (a few KB a minute), so a
    # player's slow level can be told apart (the game's own work, the GPU, shader compiles, stalls).
    env.setdefault('BB_FRAME_STATS', '1')
    # Performance work (temporary, before release): where the game's main thread was during each slow frame, and CPU
    # per thread ("Profile:" and "Threads:" lines). Measured to cost nothing visible: 80-86 slow frames with it on or
    # off in the same scripted run.
    env.setdefault('LBPPC_PROFILE', '1')
    # Readback test (temporary): guest writes into GPU-written memory skip the copy back (buffer_cache.cpp).
    env.setdefault('LBPPC_WRITE_FAULT_NO_READBACK', '1')
    # Climbing teleports (temporary): optimizations switched off while the game runs, by writing their mask
    # (gpu/shim/bbport_toggles.h, src/runtime_memory.c) to user/toggles.txt; read 4 times a second.
    env.setdefault('BB_TOGGLE_FILE', str(user / 'toggles.txt'))    # Online: LBPPC_NP_USER=<name> signs in locally as that player (no PSN or shadNet account) and
    # LBPPC_SERVER=<url> sends the game's server requests (Sony's, now closed) to a custom server, such as a
    # Refresh instance.
    # Or BB_SHADNET_SERVER=<host[:port]>, BB_SHADNET_NPID and BB_SHADNET_PASSWORD: signed in to a shadNet server (friends,
    # and the signaling that connects players to each other for multiplayer); its WebAPI (port 31315) answers the
    # game's PSN Web API calls.
    if env.get('BB_SHADNET_NPID'):
        env.setdefault('BB_ONLINE', '1')
        # Beta: sign-in, sessions, invitations and joins in the log (the online classes from info).
        env.setdefault('BB_ONLINE_LOG', '1')
        # What the server's clientversions.cfg checks (it can turn old LBPPC builds away).
        env.setdefault('BB_CLIENT_VERSION', f"LBPPC {env.get('LBPPC_VERSION', 'development build')}")
        env.setdefault('BB_P2P_PORT', '0')
        # The router forwards the P2P port to this PC (UPnP), so a joining player can reach the host: without
        # it their packets stop at the host's router. BB_UPNP=0 turns it off.
        env.setdefault('BB_UPNP', '1')
        host = env.get('BB_SHADNET_SERVER', '127.0.0.1').rsplit(':', 1)[0]
        env.setdefault('BB_SHADNET_WEBAPI', f'http://{host}:31315')
    elif env.get('LBPPC_NP_USER'):
        env.setdefault('BB_ONLINE', '1')
        # LBP3 binds its own P2P socket to the PS4's port, 3658 (its transport, which every multiplayer message,
        # its own party's included, goes through): the PSN signaling library takes another free port.
        env.setdefault('BB_P2P_PORT', '0')
    if env.get('LBPPC_SERVER') and not env.get('SHADPS4_HTTP_HOST_OVERRIDES_JSON'):
        overrides = OUT / 'host_overrides.json'
        overrides.write_text(json.dumps({'littlebigplanetps3.online.scee.com': env['LBPPC_SERVER']}), encoding='utf-8')
        env['SHADPS4_HTTP_HOST_OVERRIDES_JSON'] = str(overrides)
    if MSYS2_BIN.is_dir():
        env['PATH'] = f'{MSYS2_BIN}{os.pathsep}{env.get("PATH", "")}'
    command = [str(probe), str(OUT / 'boot-linked.bin'), '--content-profile', str(OUT / 'content.bin'),
               '--app0', str(game), '--user', str(user), '--timeout', '0', *argv[1:]]
    keep_previous_log()
    with (OUT / 'last_run.log').open('wb') as log:
        summary = system_summary()
        log.write(summary.encode())
        sys.stdout.write(summary)
        while True:
            process = subprocess.Popen(command, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                                       stderr=subprocess.STDOUT, env=env, cwd=HERE)
            for line in process.stdout:
                log.write(line)
                sys.stdout.buffer.write(line)
                sys.stdout.flush()
            code = process.wait()
            if code != RESTART_EXIT_CODE:
                break
            print('Restarting the game (settings menu)', flush=True)
    if code:
        # Windows status codes say how it ended: 0xC00000FD stack overflow, 0xC0000374 heap corruption,
        # 0xC0000409 fast fail (abort, a security check), 0xC0000005 access violation.
        print(f'Game exited with 0x{code & 0xffffffff:08X}', file=sys.stderr)
    return 0 if code == 0 else 1


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
