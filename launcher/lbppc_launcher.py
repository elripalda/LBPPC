#!/usr/bin/env python3
"""LBPPC launcher: pick the game folder (the player's own dump), choose the resolution, display mode and
frame generation, and play. Settings are kept in user/launcher.json; frame generation also goes to
bbport.ini, which the renderer reads. The game runs through run.py with the same Python.

The banner shows the dump's own artwork (sce_sys/pic1.png) when Pillow is installed; nothing of the game ships
with the launcher.

usage: python launcher/lbppc_launcher.py
"""
import json
import os
from pathlib import Path
import queue
import re
import struct
import subprocess
import sys
import threading
import tkinter as tk
from tkinter import filedialog, font as tkfont, ttk

try:
    from PIL import Image, ImageDraw, ImageFilter, ImageTk
except ImportError:  # the banner is drawn without the game's artwork
    Image = None

VERSION = '0.0.1'
# Packaged (frozen with PyInstaller): LBPPC.exe sits in the package folder and runs run.py itself (--script).
FROZEN = getattr(sys, 'frozen', False)
ROOT = Path(sys.executable).resolve().parent if FROZEN else Path(__file__).resolve().parent.parent
BIN = ROOT / 'bin' if (ROOT / 'bin' / 'bb-probe.exe').is_file() else ROOT / 'out'
USER = ROOT / 'user'
SETTINGS = USER / 'launcher.json'
INI = ROOT / 'bbport.ini'
LOG = ROOT / 'out' / 'last_run.log'

# The game renders at these sizes natively (LBPPC_RENDER_RES: its render targets are made that large).
# Other aspects widen the view (ultrawide: more of the level at the sides, the menus laid out for it). Wider than
# 4480 the game does not survive yet (probe.c render_res_install); the window scales the picture up.
RESOLUTIONS = [('1920 x 1080 (original)', '1920x1080'), ('2560 x 1440', '2560x1440'), ('3840 x 2160 (4K)', '3840x2160'),
               ('2560 x 1080 (21:9)', '2560x1080'), ('3440 x 1440 (21:9)', '3440x1440'),
               ('3840 x 1600 (21:9)', '3840x1600'), ('3840 x 1080 (32:9)', '3840x1080'),
               ('4480 x 1260 (32:9)', '4480x1260')]
# BB_WINDOW_MODE: borderless covers the desktop; exclusive takes the display mode.
DISPLAY_MODES = [('Windowed', 'windowed'), ('Borderless fullscreen', 'borderless'), ('Exclusive fullscreen', 'fullscreen')]
# bbport.ini frame_gen: the game steps 30 times a second; frame generation shows 60.
FRAME_GENS = [('Off (30 FPS)', 'off'), ('FSR 3.1 (any GPU)', 'fsr3'), ('DLSS (GeForce RTX 40/50)', 'dlss')]
# BB_LOW_LATENCY: NVIDIA Reflex (AMD Anti-Lag 2 on AMD), with the 30 FPS limit kept before each game frame.
LATENCY = [('On (NVIDIA Reflex / AMD Anti-Lag 2)', 'on'), ('On + Boost (NVIDIA)', 'boost'), ('Off', 'off')]
OPTIONS = [('resolution', 'Resolution', RESOLUTIONS, 'Wider than 16:9 shows more of the level.'),
           ('display', 'Display', DISPLAY_MODES, 'Borderless suits most screens.'),
           ('frame_gen', 'Frame generation', FRAME_GENS, 'The game runs 30 steps a second; this shows 60.'),
           ('latency', 'Low latency', LATENCY, 'Less delay between your input and the screen.')]
DEFAULTS = {'game': '', 'resolution': '1920x1080', 'display': 'windowed', 'frame_gen': 'dlss', 'latency': 'on',
            'online': False, 'server': 'http://127.0.0.1:10061', 'online_name': '', 'shadnet_password': '',
            'shadnet_server': '127.0.0.1:31313', 'join_allow': ''}
DLSS_FILES = ('bbport_dlss.dll', 'nvngx_dlss.dll', 'nvngx_dlssg.dll')

# Warm, craft-paper colours.
BG, PANEL, EDGE, FIELD = '#17130f', '#221d17', '#3a3127', '#2c251d'
TEXT, MUTED, ACCENT, ACCENT_HOT, GOOD, ERROR = '#f4ead8', '#b5a78f', '#f2a33a', '#ffb957', '#9ccc65', '#ff7b6b'
WIDTH, BANNER_HEIGHT = 860, 230


def read_sfo(path):
    """The strings and numbers of a param.sfo, by key; empty when it cannot be read."""
    try:
        data = Path(path).read_bytes()
        magic, _version, key_table, data_table, count = struct.unpack_from('<4sIIII', data, 0)
        if magic != b'\0PSF':
            return {}
        values = {}
        for i in range(count):
            key_offset, fmt, length, _max, data_offset = struct.unpack_from('<HHIII', data, 20 + i * 16)
            key = data[key_table + key_offset:data.index(b'\0', key_table + key_offset)].decode('ascii', 'replace')
            raw = data[data_table + data_offset:data_table + data_offset + length]
            values[key] = struct.unpack('<I', raw)[0] if fmt == 0x0404 and length == 4 else \
                raw.rstrip(b'\0').decode('utf-8', 'replace')
        return values
    except (OSError, ValueError, struct.error):
        return {}


def check_game(folder):
    """(ok, description) of a game folder."""
    if not folder:
        return False, 'Choose the folder of your game dump (the one holding eboot.bin).'
    game = Path(folder)
    if not (game / 'eboot.bin').is_file():
        return False, 'No eboot.bin in this folder.'
    sfo = read_sfo(game / 'sce_sys' / 'param.sfo')
    title_id, version = sfo.get('TITLE_ID', '?'), sfo.get('APP_VER', '?')
    if title_id not in ('CUSA00473', '?'):
        return True, f'{title_id} version {version}: other regions or editions are untested.'
    return True, f'{title_id} version {version}, ready to play'


def load_settings():
    try:
        values = json.loads(SETTINGS.read_text(encoding='utf-8'))
        return {**DEFAULTS, **{k: v for k, v in values.items() if k in DEFAULTS}}
    except (OSError, ValueError):
        return dict(DEFAULTS)


def save_settings(values):
    USER.mkdir(exist_ok=True)
    SETTINGS.write_text(json.dumps(values, indent=2), encoding='utf-8')
    # bbport.ini: frame_gen set, its other lines kept.
    lines = INI.read_text(encoding='utf-8').splitlines() if INI.is_file() else []
    lines = [line for line in lines if not line.startswith('frame_gen=')]
    lines.append(f"frame_gen={values['frame_gen']}")
    INI.write_text('\n'.join(lines) + '\n', encoding='utf-8')


def open_path(path):
    if not path.suffix:
        path.mkdir(parents=True, exist_ok=True)
    if os.name == 'nt':
        os.startfile(str(path))  # noqa: S606 - opens the folder or log in Explorer / the text editor
    else:
        subprocess.Popen(['xdg-open', str(path)])


def banner_image(game):
    """The banner: the dump's pic1.png faded into the background (a plain warm gradient without it)."""
    width, height = WIDTH, BANNER_HEIGHT
    art = Path(game) / 'sce_sys' / 'pic1.png' if game else None
    try:
        picture = Image.open(art).convert('RGB') if art and art.is_file() else None
    except OSError:
        picture = None
    if picture:
        scale = width / picture.width
        picture = picture.resize((width, round(picture.height * scale)), Image.LANCZOS)
        top = max(0, (picture.height - height) // 3)
        base = picture.crop((0, top, width, top + height))
    else:
        base = Image.new('RGB', (width, height), PANEL)
        glow = Image.new('RGB', (width, height), ACCENT)
        mask = Image.new('L', (width, height))
        ImageDraw.Draw(mask).ellipse((width * 0.55, -height, width * 1.4, height * 1.2), fill=90)
        base = Image.composite(glow, base, mask.filter(ImageFilter.GaussianBlur(60)))
    # Darkened towards the bottom and the left, where the title sits.
    shade = Image.new('L', (width, height))
    pixels = shade.load()
    for y in range(height):
        for x in range(0, width, 4):
            v = int(255 * min(1.0, 0.25 + 0.75 * (y / height) ** 1.6 + 0.45 * max(0.0, 1 - x / (width * 0.6))))
            for dx in range(4):
                if x + dx < width:
                    pixels[x + dx, y] = v
    dark = Image.new('RGB', (width, height), BG)
    return Image.composite(dark, base, shade)


class PillButton(tk.Canvas):
    """A rounded button drawn on a canvas (ttk buttons are square)."""

    def __init__(self, parent, text, command, fill, hot, fg='#1b140c', width=150, height=44, size=12):
        super().__init__(parent, width=width, height=height, bg=parent['bg'], highlightthickness=0, cursor='hand2')
        self.command, self.fill, self.hot, self.fg, self.enabled = command, fill, hot, fg, True
        self.w, self.h, self.text = width, height, text
        self.font = tkfont.Font(family='Segoe UI Semibold', size=size)
        self.bind('<Enter>', lambda _e: self.draw(hover=True))
        self.bind('<Leave>', lambda _e: self.draw())
        self.bind('<Button-1>', lambda _e: self.enabled and self.command())
        self.draw()

    def draw(self, hover=False):
        self.delete('all')
        fill = (self.hot if hover else self.fill) if self.enabled else EDGE
        r, w, h = self.h // 2, self.w, self.h
        if Image:
            # Drawn four times larger and scaled down: smooth edges.
            pill = Image.new('RGB', (w * 4, h * 4), self['bg'])
            ImageDraw.Draw(pill).rounded_rectangle((0, 0, w * 4 - 1, h * 4 - 1), radius=r * 4, fill=fill)
            self.pill = ImageTk.PhotoImage(pill.resize((w, h), Image.LANCZOS))
            self.create_image(0, 0, image=self.pill, anchor='nw')
        else:
            self.create_oval(0, 0, 2 * r, h, fill=fill, outline=fill)
            self.create_oval(w - 2 * r - 1, 0, w - 1, h, fill=fill, outline=fill)
            self.create_rectangle(r, 0, w - r, h, fill=fill, outline=fill)
        self.create_text(w / 2, h / 2, text=self.text, fill=self.fg if self.enabled else MUTED, font=self.font)

    def set_enabled(self, enabled):
        self.enabled = enabled
        self.configure(cursor='hand2' if enabled else 'arrow')
        self.draw()


class Launcher(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title(f'LBPPC {VERSION}')
        self.configure(bg=BG)
        self.resizable(False, True)
        self.minsize(WIDTH, 560)
        self.settings = load_settings()
        self.process = None
        self.lines = queue.Queue()
        self.log_shown = False
        self.banner_art = None
        self.style()
        self.build()
        self.refresh_game()
        self.protocol('WM_DELETE_WINDOW', self.close)
        self.after(100, self.pump)

    def style(self):
        style = ttk.Style(self)
        style.theme_use('clam')
        style.configure('.', background=BG, foreground=TEXT, fieldbackground=FIELD, bordercolor=EDGE,
                        lightcolor=EDGE, darkcolor=EDGE, font=('Segoe UI', 10))
        style.configure('Card.TFrame', background=PANEL)
        style.configure('Card.TLabel', background=PANEL, foreground=TEXT, font=('Segoe UI Semibold', 10))
        style.configure('Hint.TLabel', background=PANEL, foreground=MUTED, font=('Segoe UI', 9))
        style.configure('Status.TLabel', background=BG, foreground=MUTED, font=('Segoe UI', 10))
        style.configure('Link.TLabel', background=BG, foreground=MUTED, font=('Segoe UI', 10, 'underline'))
        style.configure('TEntry', padding=6, insertcolor=TEXT)
        style.configure('TCombobox', padding=5, arrowcolor=ACCENT, foreground=TEXT, background=FIELD,
                        selectbackground=FIELD, selectforeground=TEXT)
        style.map('TCombobox', fieldbackground=[('readonly', FIELD)], foreground=[('readonly', TEXT)],
                  background=[('active', EDGE)])
        style.configure('Browse.TButton', background=FIELD, foreground=TEXT, padding=(12, 5), borderwidth=0)
        style.map('Browse.TButton', background=[('active', EDGE)])
        style.configure('Vertical.TScrollbar', background=PANEL, troughcolor=BG, arrowcolor=MUTED, borderwidth=0)
        self.option_add('*TCombobox*Listbox.background', FIELD)
        self.option_add('*TCombobox*Listbox.foreground', TEXT)
        self.option_add('*TCombobox*Listbox.selectBackground', ACCENT)
        self.option_add('*TCombobox*Listbox.selectForeground', '#1b140c')
        self.option_add('*TCombobox*Listbox.font', ('Segoe UI', 10))

    def build(self):
        # Banner: artwork, title and the stitched edge under it.
        self.banner = tk.Canvas(self, width=WIDTH, height=BANNER_HEIGHT, bg=BG, highlightthickness=0)
        self.banner.pack(fill='x')

        body = tk.Frame(self, bg=BG, padx=28)
        body.pack(fill='both', expand=True)

        game = ttk.Frame(body, style='Card.TFrame', padding=(18, 14))
        game.pack(fill='x', pady=(14, 10))
        ttk.Label(game, text='Game folder', style='Card.TLabel').grid(row=0, column=0, sticky='w')
        self.game = tk.StringVar(value=self.settings['game'])
        entry = ttk.Entry(game, textvariable=self.game, font=('Segoe UI', 10))
        entry.grid(row=1, column=0, sticky='ew', pady=(6, 0))
        entry.bind('<FocusOut>', lambda _e: self.refresh_game())
        entry.bind('<Return>', lambda _e: self.refresh_game())
        ttk.Button(game, text='Browse…', style='Browse.TButton', command=self.browse).grid(
            row=1, column=1, padx=(10, 0), pady=(6, 0))
        self.game_status = ttk.Label(game, style='Hint.TLabel')
        self.game_status.grid(row=2, column=0, columnspan=2, sticky='w', pady=(8, 0))
        game.columnconfigure(0, weight=1)

        options = ttk.Frame(body, style='Card.TFrame', padding=(18, 14))
        options.pack(fill='x')
        self.choices = {}
        for index, (key, label, values, hint) in enumerate(OPTIONS):
            cell = ttk.Frame(options, style='Card.TFrame')
            cell.grid(row=index // 2, column=index % 2, sticky='nsew', padx=(0 if index % 2 == 0 else 14, 0),
                      pady=(0 if index < 2 else 12, 0))
            ttk.Label(cell, text=label, style='Card.TLabel').pack(anchor='w')
            names = [name for name, _value in values]
            current = next((name for name, value in values if value == self.settings[key]), names[0])
            var = tk.StringVar(value=current)
            box = ttk.Combobox(cell, textvariable=var, values=names, state='readonly', font=('Segoe UI', 10))
            box.pack(fill='x', pady=(6, 4))
            box.bind('<<ComboboxSelected>>', lambda _e: (self.store(), self.selection_clear()))
            ttk.Label(cell, text=hint, style='Hint.TLabel').pack(anchor='w')
            self.choices[key] = (var, values)
        options.columnconfigure(0, weight=1, uniform='col')
        options.columnconfigure(1, weight=1, uniform='col')

        # Online: a custom server (Refresh or Lighthouse), signed in locally under an online name.
        online = ttk.Frame(body, style='Card.TFrame', padding=(18, 14))
        online.pack(fill='x', pady=(10, 0))
        self.online = tk.BooleanVar(value=bool(self.settings['online']))
        tk.Checkbutton(online, text='Play online', variable=self.online, command=self.store, bg=PANEL, fg=TEXT,
                       selectcolor=FIELD, activebackground=PANEL, activeforeground=TEXT,
                       font=('Segoe UI Semibold', 10), bd=0, highlightthickness=0).grid(row=0, column=0, sticky='w')
        ttk.Label(online, text='Online name', style='Card.TLabel').grid(row=1, column=0, sticky='w', pady=(8, 0))
        ttk.Label(online, text='Server', style='Card.TLabel').grid(row=1, column=1, sticky='w', padx=(14, 0), pady=(8, 0))
        self.online_name = tk.StringVar(value=self.settings['online_name'])
        self.server = tk.StringVar(value=self.settings['server'])
        for col, var in ((0, self.online_name), (1, self.server)):
            entry = ttk.Entry(online, textvariable=var, font=('Segoe UI', 10))
            entry.grid(row=2, column=col, sticky='ew', padx=(0 if col == 0 else 14, 0), pady=(6, 4))
            entry.bind('<FocusOut>', lambda _e: self.store())
        ttk.Label(online, text='Your account name on that server (up to 16 letters, digits, - or _).',
                  style='Hint.TLabel').grid(row=3, column=0, columnspan=2, sticky='w')
        ttk.Label(online, text='shadNet password', style='Card.TLabel').grid(row=4, column=0, sticky='w', pady=(10, 0))
        ttk.Label(online, text='shadNet server', style='Card.TLabel').grid(row=4, column=1, sticky='w', padx=(14, 0),
                                                                        pady=(10, 0))
        self.shadnet_password = tk.StringVar(value=self.settings['shadnet_password'])
        self.shadnet_server = tk.StringVar(value=self.settings['shadnet_server'])
        for col, var, hidden in ((0, self.shadnet_password, True), (1, self.shadnet_server, False)):
            entry = ttk.Entry(online, textvariable=var, font=('Segoe UI', 10), show='*' if hidden else '')
            entry.grid(row=5, column=col, sticky='ew', padx=(0 if col == 0 else 14, 0), pady=(6, 4))
            entry.bind('<FocusOut>', lambda _e: self.store())
        ttk.Label(online, text='For multiplayer: your shadNet account (same name). Empty: playing alone online.',
                  style='Hint.TLabel').grid(row=6, column=0, columnspan=2, sticky='w')
        ttk.Label(online, text='Who may join you', style='Card.TLabel').grid(row=7, column=0, sticky='w', pady=(10, 0))
        self.join_allow = tk.StringVar(value=self.settings['join_allow'])
        entry = ttk.Entry(online, textvariable=self.join_allow, font=('Segoe UI', 10))
        entry.grid(row=8, column=0, columnspan=2, sticky='ew', pady=(6, 4))
        entry.bind('<FocusOut>', lambda _e: self.store())
        ttk.Label(online, text='Your shadNet friends, and these online names (comma separated). Nobody else can '
                               'connect to your game.', style='Hint.TLabel').grid(row=9, column=0, columnspan=2,
                                                                               sticky='w')
        online.columnconfigure(0, weight=1, uniform='col')
        online.columnconfigure(1, weight=1, uniform='col')

        bar = tk.Frame(body, bg=BG)
        bar.pack(fill='x', pady=(16, 10))
        self.play_button = PillButton(bar, 'Play', self.play, ACCENT, ACCENT_HOT, width=170, height=48, size=14)
        self.play_button.pack(side='left')
        self.stop_button = PillButton(bar, 'Stop', self.stop, FIELD, EDGE, fg=TEXT, width=100, height=48, size=11)
        self.stop_button.pack(side='left', padx=(10, 0))
        self.stop_button.set_enabled(False)
        self.state_dot = tk.Canvas(bar, width=12, height=12, bg=BG, highlightthickness=0)
        self.state_dot.pack(side='left', padx=(18, 6))
        self.state_label = ttk.Label(bar, text='Ready', style='Status.TLabel')
        self.state_label.pack(side='left')
        self.set_state('Ready', MUTED)
        for text, action in (('Saves', lambda: open_path(USER)), ('Log file', lambda: LOG.is_file() and open_path(LOG)),
                             ('Show log', self.toggle_log)):
            link = ttk.Label(bar, text=text, style='Link.TLabel', cursor='hand2')
            link.pack(side='right', padx=(14, 0))
            link.bind('<Button-1>', lambda _e, act=action: act())
            if text == 'Show log':
                self.log_link = link

        self.log_frame = tk.Frame(body, bg=BG)
        self.log = tk.Text(self.log_frame, height=10, bg='#100d0a', fg=MUTED, insertbackground=TEXT, relief='flat',
                           font=('Consolas', 9), wrap='none', state='disabled', padx=10, pady=8,
                           highlightthickness=1, highlightbackground=EDGE, highlightcolor=EDGE)
        scroll = ttk.Scrollbar(self.log_frame, command=self.log.yview)
        self.log.configure(yscrollcommand=scroll.set)
        self.log.pack(side='left', fill='both', expand=True)
        scroll.pack(side='right', fill='y')
        self.log.tag_configure('warn', foreground=ACCENT)
        self.log.tag_configure('bad', foreground=ERROR)

        tk.Label(self, text='Play your own copy. LBPPC includes no game files.', bg=BG, fg=EDGE,
                 font=('Segoe UI', 8)).pack(side='bottom', pady=(0, 8))

    def draw_banner(self):
        canvas = self.banner
        canvas.delete('all')
        if Image:
            self.banner_art = ImageTk.PhotoImage(banner_image(self.settings['game']))
            canvas.create_image(0, 0, image=self.banner_art, anchor='nw')
        sfo = read_sfo(Path(self.settings['game']) / 'sce_sys' / 'param.sfo') if self.settings['game'] else {}
        canvas.create_text(30, BANNER_HEIGHT - 92, text='LBPPC', anchor='w', fill=TEXT,
                           font=('Segoe UI Black', 34))
        canvas.create_text(33, BANNER_HEIGHT - 52, anchor='w', fill=MUTED, font=('Segoe UI', 11),
                           text=sfo.get('TITLE', 'Your PS4 game, on PC'))
        # A stitched seam along the bottom edge.
        y = BANNER_HEIGHT - 14
        canvas.create_line(28, y, WIDTH - 28, y, fill=ACCENT, width=2, dash=(10, 7), capstyle='round')

    def browse(self):
        folder = filedialog.askdirectory(title='Game folder (holding eboot.bin)', initialdir=self.game.get() or None)
        if folder:
            self.game.set(folder)
            self.refresh_game()

    def refresh_game(self):
        ok, text = check_game(self.game.get().strip())
        self.game_status.configure(text=('✓  ' if ok else '!  ') + text, foreground=GOOD if ok else ERROR)
        self.play_button.set_enabled(ok and not self.process)
        changed = self.settings['game'] != self.game.get().strip()
        self.store()
        if changed or not self.banner.find_all():
            self.draw_banner()
        return ok

    def store(self):
        self.settings['game'] = self.game.get().strip()
        for key, (var, values) in self.choices.items() if hasattr(self, 'choices') else ():
            self.settings[key] = next(value for name, value in values if name == var.get())
        if hasattr(self, 'online'):
            self.settings['online'] = bool(self.online.get())
            self.settings['online_name'] = self.online_name.get().strip()
            self.settings['server'] = self.server.get().strip()
            self.settings['shadnet_password'] = self.shadnet_password.get()
            self.settings['shadnet_server'] = self.shadnet_server.get().strip()
            self.settings['join_allow'] = self.join_allow.get().strip()
        save_settings(self.settings)

    def set_state(self, text, colour):
        self.state_label.configure(text=text)
        self.state_dot.delete('all')
        self.state_dot.create_oval(1, 1, 11, 11, fill=colour, outline=colour)

    def toggle_log(self):
        self.log_shown = not self.log_shown
        if self.log_shown:
            self.log_frame.pack(fill='both', expand=True, pady=(0, 6))
            self.log.see('end')
        else:
            self.log_frame.pack_forget()
        self.log_link.configure(text='Hide log' if self.log_shown else 'Show log')

    def play(self):
        if self.process or not self.refresh_game():
            return
        self.store()
        env = dict(os.environ)
        env['LBPPC_RENDER_RES'] = self.settings['resolution']
        if self.settings['display'] != 'windowed':
            env['BB_WINDOW_MODE'] = self.settings['display']
        else:
            env.pop('BB_WINDOW_MODE', None)
        if self.settings['latency'] == 'off':
            env.update(BB_LOW_LATENCY='off', BB_VBLANK_HZ='60', BB_FPS_LIMIT='-1')
        else:
            env.update(BB_LOW_LATENCY=self.settings['latency'], BB_VBLANK_HZ='0', BB_FPS_LIMIT='30')
        for name in ('LBPPC_NP_USER', 'LBPPC_SERVER', 'BB_SHADNET_NPID', 'BB_SHADNET_PASSWORD', 'BB_SHADNET_SERVER'):
            env.pop(name, None)
        if self.settings['online']:
            name, server = self.settings['online_name'], self.settings['server']
            if not re.fullmatch(r'[A-Za-z0-9_-]{3,16}', name) or not server.startswith(('http://', 'https://')):
                self.set_state('Online needs a name (3 to 16 letters, digits, - or _) and a server address', ERROR)
                return
            env.update(LBPPC_SERVER=server.rstrip('/'))
            if self.settings['shadnet_password']:
                env.update(BB_SHADNET_NPID=name, BB_SHADNET_PASSWORD=self.settings['shadnet_password'],
                           BB_SHADNET_SERVER=self.settings['shadnet_server'] or '127.0.0.1:31313',
                           LBPPC_JOIN_ALLOW=self.settings['join_allow'])
            else:
                env.update(LBPPC_NP_USER=name)
        env['PYTHONUNBUFFERED'] = '1'
        flags = subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0
        self.clear_log()
        if self.settings['frame_gen'] == 'dlss' and not all((BIN / name).is_file() for name in DLSS_FILES):
            # The game would quietly use FSR 3.1 instead (packaging/windows/import_dlss.py installs them).
            self.append(['Launcher: the DLSS files are missing next to bb-probe.exe; frame generation uses FSR 3.1.'])
        command = [sys.executable, '--script'] if FROZEN else [sys.executable]
        self.process = subprocess.Popen(command + [str(ROOT / 'run.py'), self.settings['game']], cwd=ROOT,
                                        env=env, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                                        stderr=subprocess.STDOUT, creationflags=flags)
        threading.Thread(target=self.read_output, args=(self.process,), daemon=True).start()
        self.play_button.set_enabled(False)
        self.stop_button.set_enabled(True)
        self.set_state('Starting…', ACCENT)

    def read_output(self, process):
        for raw in process.stdout:
            self.lines.put(raw.decode('utf-8', 'replace').rstrip('\r\n'))
        self.lines.put((process.wait(),))

    def pump(self):
        shown = []
        try:
            while len(shown) < 400:
                item = self.lines.get_nowait()
                if isinstance(item, tuple):
                    self.finished(item[0])
                    break
                shown.append(item)
                if 'Frame rate' in item or 'Pipeline cache' in item:
                    self.set_state('Running', GOOD)
        except queue.Empty:
            pass
        if shown:
            self.append(shown)
        self.after(100, self.pump)

    def append(self, lines):
        self.log.configure(state='normal')
        for line in lines:
            tag = 'bad' if line.startswith(('Fault', 'STOP', 'Game exited')) else \
                'warn' if line.startswith(('Launcher:', 'LBPPC_')) else ()
            self.log.insert('end', line + '\n', tag)
        excess = int(self.log.index('end-1c').split('.')[0]) - 2000
        if excess > 0:
            self.log.delete('1.0', f'{excess}.0')
        self.log.see('end')
        self.log.configure(state='disabled')

    def clear_log(self):
        self.log.configure(state='normal')
        self.log.delete('1.0', 'end')
        self.log.configure(state='disabled')

    def finished(self, code):
        self.process = None
        self.stop_button.set_enabled(False)
        if code == 0:
            self.set_state('Closed', MUTED)
        else:
            self.set_state('Ended with an error: see the log', ERROR)
            if not self.log_shown:
                self.toggle_log()
        self.refresh_game()

    def stop(self):
        if self.process:
            # run.py's game process is its child: the whole tree ends (on Windows before run.py itself,
            # whose children taskkill finds through it).
            if os.name == 'nt':
                subprocess.run(['taskkill', '/T', '/F', '/PID', str(self.process.pid)], capture_output=True)
            self.process.terminate()

    def close(self):
        self.stop()
        self.destroy()


def run_script(argv):
    """LBPPC.exe --script <file.py> [args]: runs a Python script of the package (run.py, scripts/, tools/) with the
    Python frozen into LBPPC.exe, so players need no Python of their own."""
    import runpy
    path = Path(argv[0]).resolve()
    sys.argv = [str(path), *argv[1:]]
    sys.path.insert(0, str(path.parent))
    runpy.run_path(str(path), run_name='__main__')


if __name__ == '__main__':
    if len(sys.argv) > 2 and sys.argv[1] == '--script':
        run_script(sys.argv[2:])
    else:
        Launcher().mainloop()
