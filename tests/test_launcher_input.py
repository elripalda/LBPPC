"""launcher/input_settings.py against the game's parser (src/runtime_input_config.c): the same names, the same
default text, and files it writes that the parser takes without a warning.
usage: python tests/test_launcher_input.py   (run from the repository)"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'launcher'))
import input_settings as I  # noqa: E402

C = (ROOT / 'src' / 'runtime_input_config.c').read_text(encoding='utf-8')


def table(name):
    body = re.search(r'static const \w+ ' + name + r'\[\] = \{(.*?)\n\};', C, re.S).group(1)
    return set(re.findall(r'\{"([a-z0-9_]+)"', body))


def default_text():
    """The C default text: DEFAULT_* macros' string literals, in the order input_config_default_text joins them."""
    order = re.search(r'return (DEFAULT_\w+(?:\s+DEFAULT_\w+)*);', C).group(1).split()
    text = ''
    for macro in order:
        body = re.search(r'#define ' + macro + r' \\\n(.*?)\n\n', C + '\n\n', re.S).group(1)
        for literal in re.findall(r'"((?:[^"\\]|\\.)*)"', body):
            text += literal.encode().decode('unicode_escape')
    return text


def check(condition, message):
    if not condition:
        print('FAIL:', message)
        sys.exit(1)
    print('PASS:', message)


keys, mouse = table('kKeyNames'), table('kMouseButtonNames') | table('kMouseWheelNames')
pad = table('kCButtonNames') | table('kAxisNames') | table('kHalfAxisNames')
outputs = table('kOutputNames')
settings = {'mouse_to_joystick', 'mouse_movement_params', 'mouse_invert_x', 'mouse_invert_y', 'mouse_sensitivity_y',
            'analog_deadzone', 'stick_socd', 'stick_release_delay_ms', 'stick_circular', 'stick_turn_rate'}

check(set(I.KEY_NAMES) == keys, 'key names match kKeyNames')
check(set(I.MOUSE_NAMES) == mouse, 'mouse names match kMouseButtonNames and kMouseWheelNames')
check(set(I.PAD_NAMES) <= pad, 'controller names are the parser\'s')
check(I.ACTION_OUTPUTS <= outputs, 'actions are outputs the parser knows')
check(I.DEFAULT_TEXT == default_text(), 'DEFAULT_TEXT is runtime_input_config.c\'s default text')

# What the page writes: every line a known output or setting, with known inputs.
model = I.InputSettings(I.DEFAULT_TEXT)
model.mouse_mode, model.invert_y, model.vertical = 'touchpad', True, 1.5
model.speed = 2.25
written = model.text()
for line in written.splitlines():
    if not line.strip() or line.startswith('#'):
        continue
    output, value = (part.strip() for part in line.split('=', 1))
    known_output = output in outputs | settings
    known_input = (output in settings or value in keys | mouse | pad | {'unmapped'})
    if not (known_output and known_input):
        check(False, f'written line is valid: {line}')
check(True, 'every written line is a known output or setting with known inputs')

# Read back, nothing is lost.
again = I.InputSettings(written)
check(again.keys == model.keys and again.pad == model.pad, 'bindings survive writing and reading')
check((again.mouse_mode, again.speed, again.invert_y, again.vertical) == ('touchpad', 2.25, True, 1.5),
      'mouse settings survive writing and reading')
check('hotkey_toggle_mouse_to_joystick = f7' in again.other and 'axis_left_x = axis_left_x' in again.other,
      'lines the page does not edit are kept')
check(I.InputSettings(written).text() == written, 'writing is stable')

# The defaults as the page sees them.
defaults = I.InputSettings()
check(defaults.keys['cross'] == ['space'] and defaults.pad['cross'] == ['cross'], 'Cross: Space and Cross')
check(defaults.keys['r3'] == ['q', 'c'] and defaults.keys['r1'] == ['3', 'leftbutton'], 'two keys per action')
check(defaults.pad['axis_left_x_minus'] == [], 'stick directions take no controller binding')

# A key bound again moves from the action that had it.
moved = defaults.bind_key('circle', 0, 'space')
check(moved == 'cross' and defaults.keys['cross'] == [] and defaults.keys['circle'][0] == 'space',
      'a key moves from its old action')
defaults.clear_key('circle', 0)
check('space' not in defaults.keys['circle'], 'a key can be cleared')
defaults.set_pad('triangle', 'l1')
check(defaults.pad['triangle'] == ['l1'], 'controller input changed')

# Combos and unknown lines are kept, not lost.
odd = I.InputSettings('cross = lctrl, a\nstick_socd = neutral\ncross = space\n')
check('cross = lctrl, a' in odd.other and 'stick_socd = neutral' in odd.other and odd.keys['cross'] == ['space'],
      'combos and other settings are kept')
print('launcher input tests passed')
