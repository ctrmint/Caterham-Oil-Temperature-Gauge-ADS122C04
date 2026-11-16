"""Write an option's parts lists, its zipped Gerbers and its output manifest. Stdlib only.

    outputs.py bom <a|b> <option_dir>            BOM.md, veroboard/option-X-veroboard-bom.csv, pcb/option-X-pcb-bom.csv
    outputs.py gerbers <plot_dir> <zip_path>     the plots zipped, with their timestamps fixed (reproducible)
    outputs.py manifest <option_dir>             outputs.sha256 for every generated file in the option's folder

build.sh calls these.
"""
import csv
import hashlib
import io
import os
import re
import sys
import zipfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import circuit as C  # noqa: E402

MANIFEST_SKIP = {'outputs.sha256', 'README.md'}
I = C.PARTS_INFO

# (qty, ref, item, info key or (maker, part number, Digi-Key UK, notes))
_SPEC = ('', '', '')


def every_build(option):
    rows = [
        ('1', 'U1', 'ADC module', I['module']),
        ('1', 'Rref', 'Reference resistor', I['rref']),
        ('2', 'R1, R2', 'Tap resistors', I['r2k2']),
    ]
    if option == 'b':
        rows.append(('2', 'C1, C2', 'Filter capacitors', I['c47n']))
    rows += [
        ('1', '', 'Display', ('Waveshare', 'ESP32-S3-Touch-AMOLED-1.75', '',
                              'Standard or -B; not 1.75C, 2.1 LCD or GNSS -G. Check the board revision and socket first')),
        ('1', '', 'Display plug', I['display_plug']),
        ('1', '', 'Display power', _SPEC + ('Ignition-switched USB-C charger rated for the car\'s transients, and a '
                                            'secured USB-C lead. Never 12 V to the display',)),
        ('as needed', '', 'Harness wire', _SPEC + ('0.5 mm2 (20 AWG): signal, ground sense and return, twisted '
                                                   'together, away from ignition leads, the alternator and the exhaust',)),
        ('1', '', 'Ring terminal', _SPEC + ('For the tank bond stud: the ground-sense and return wires land on it',)),
        ('1', '', 'Sender spade connector', _SPEC + ('Insulated, retained against vibration; check it fits the sender',)),
        ('1', '', 'Box', _SPEC + ('Small, insulated, with strain reliefs or glands for both cables',)),
        ('-', '', 'Heat-shrink and sleeving', _SPEC + ('For the harness and display-lead joints',)),
        ('1', '', 'Sender', _SPEC + ('The car\'s existing resistive oil-temperature sender (dry-sump tank). Confirm its '
                                     'type before relying on it',)),
    ]
    return rows


NO_BOARD = [
    ('1', '', '3-way screw terminal strip', _SPEC + ('A "chocolate block" cut to three ways: SENDER, SENSE, RETURN',)),
    ('1', '', 'Display lead', _SPEC + ('5-core flexible cable, 20 cm or less: 3.3 V, GND, SDA, SCL, and GND to RETURN',)),
    ('6', '', 'Module header pins', _SPEC + ('2.54 mm header for the module\'s system row; a strip comes with the module',)),
    ('-', '', 'Bootlace ferrules', _SPEC + ('For the harness wires in the terminal strip',)),
]
STRIPBOARD = [
    ('1', '', 'Stripboard', _SPEC + ('0.1 in (2.54 mm) copper stripboard, at least 29 x 11 holes; cut to size as in '
                                     'veroboard/ASSEMBLY.md',)),
    ('2', 'J1, J2', 'Terminal blocks', I['mpt4']),
    ('12', '', 'Module header pins', _SPEC + ('2.54 mm header, both rows; a strip comes with the module',)),
    ('10 cm', '', 'Tinned copper wire', _SPEC + ('0.6 mm (22 SWG), for the three wire links',)),
    ('1', '', 'Display lead', _SPEC + ('4-core flexible cable, 20 cm or less: 3.3 V, GND, SCL, SDA (J1 pins 1-4)',)),
    ('4', '', 'Bootlace ferrules', _SPEC + ('0.25 mm2, no collar, for the display lead. Phoenix lists 0.25-0.34 mm2 '
                                            'ferrules for MPT 0,5; strip 0.5 mm2 harness wire 4.5 mm and fit it bare',)),
]
PCB = [
    ('1', '', 'PCB', _SPEC + ('Order from pcb/fab/option-X-gerbers.zip: 2 layers, 1.6 mm FR-4, 1 oz copper, plated '
                              'holes, any standard prototype service',)),
    ('1', 'J1', 'Display header', I['mf2x3']),
    ('1', '', 'Display lead housing', I['mf2x3_housing']),
    ('1', 'J2', 'Harness header', I['mf1x4']),
    ('1', '', 'Harness housing', I['mf1x4_housing']),
    ('9 (+spares)', '', 'Crimp terminals', I['mf_crimp']),
    ('1', '', 'Crimp tool', I['mf_tool']),
    ('12', '', 'Module header pins', _SPEC + ('2.54 mm header, both rows; a strip comes with the module',)),
    ('1', '', 'Display lead', _SPEC + ('3 twisted pairs, 24 AWG stranded, 20 cm or less: 3.3V+GND, SCL+GND, SDA+GND. '
                                       'The three grounds join at the display plug\'s G pin',)),
    ('4', '', 'Module standoffs', _SPEC + ('M2.5 x 2.5 mm nylon spacers with M2.5 nylon screws and nuts, under the '
                                           'module\'s four holes',)),
    ('4', '', 'Board screws', _SPEC + ('M3, into the box\'s bosses (holes 64 x 29 mm apart)',)),
]
TEST_GEAR = [
    ('-', '', 'Bench test gear', _SPEC + ('Multimeter; known resistors from 44 ohm to 5 kohm, plus 10, 1, 27 and 12 ohm and '
                                          'a 1.5 V AA cell for the ground-offset test; a current-limited bench supply for '
                                          'the short-to-battery test; a reference thermometer for calibration',)),
]


def cell(s):
    return str(s).replace('|', '\\|')


def table(rows, option):
    out = ['| Qty | Ref | Item | Maker | Part number | Digi-Key UK | Notes |', '|---|---|---|---|---|---|---|']
    for qty, ref, item, (mf, mpn, code, notes) in rows:
        notes = notes.replace('option-X', f'option-{option}')
        out.append(f'| {qty} | {ref} | {cell(item)} | {cell(mf) or "any"} | {cell(mpn) or "-"} | {cell(code) or "-"} '
                   f'| {cell(notes)} |')
    return out


def write_csv(path, rows, option):
    with open(path, 'w', newline='', encoding='utf-8') as f:
        w = csv.writer(f, lineterminator='\n')
        w.writerow(['Qty', 'Ref', 'Item', 'Manufacturer', 'MPN', 'Digi-Key UK', 'Notes'])
        for qty, ref, item, (mf, mpn, code, notes) in rows:
            w.writerow([qty, ref, item, mf, mpn, code, notes.replace('option-X', f'option-{option}')])


def write_bom(option, option_dir):
    up = option.upper()
    common = every_build(option)
    md = [f'# Option {up}: parts list', '',
          f'Every part for option {up}, whichever way you build it: take the parts every build needs, then the parts '
          'for one of the three builds. The CSV files in `veroboard/` and `pcb/` are the same lists for those builds.', '',
          'Part numbers are given only where they were checked against the maker\'s data and Digi-Key UK\'s product '
          'page (2026-11-15). Everything else is a specification: any part that meets it will do. Prices and order '
          'quantities are deliberately not guessed.', '',
          'Generated by `scripts/boards/outputs.py` from `scripts/boards/circuit.py`: edit those, not this file, then '
          'run `scripts/boards/build.sh`.', '',
          '## Every build', ''] + table(common, option) + [
          '', '## Build 1: without a board', '',
          'Three resistors' + (' and two capacitors' if option == 'b' else '') + ' soldered to the module\'s pins, as '
          'in [hardware/WIRING.md](../WIRING.md) ("Building it without a board").', ''] + table(NO_BOARD, option) + [
          '', '## Build 2: stripboard', '',
          'The layout and assembly sheet are in [veroboard/](veroboard/).', ''] + table(STRIPBOARD, option) + [
          '', '## Build 3: PCB', '',
          'The KiCad project, Gerbers and renders are in [pcb/](pcb/).', ''] + table(PCB, option) + [
          '', '## Bench testing', ''] + table(TEST_GEAR, option)
    with open(os.path.join(option_dir, 'BOM.md'), 'w', encoding='utf-8') as f:
        f.write('\n'.join(md) + '\n')
    write_csv(os.path.join(option_dir, 'veroboard', f'option-{option}-veroboard-bom.csv'), common + STRIPBOARD, option)
    write_csv(os.path.join(option_dir, 'pcb', f'option-{option}-pcb-bom.csv'), common + PCB, option)


STAMP = re.compile(r'\d{4}-\d{2}-\d{2}[T ]\d{2}:\d{2}:\d{2}(?:[+-]\d{2}:?\d{2}|Z)?')


def zip_gerbers(plot_dir, zip_path):
    """Zip the plots with their plot-time stamps replaced by the design date and fixed zip timestamps, so an
    unchanged board zips identically. The files stay valid Gerber X2, Excellon and JSON."""
    buf = io.BytesIO()
    with zipfile.ZipFile(buf, 'w', zipfile.ZIP_DEFLATED) as z:
        for name in sorted(os.listdir(plot_dir)):
            text = STAMP.sub(f'{C.DATE}T00:00:00', open(os.path.join(plot_dir, name), encoding='utf-8').read())
            info = zipfile.ZipInfo(name, date_time=(1980, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o644 << 16
            z.writestr(info, text)
    with open(zip_path, 'wb') as f:
        f.write(buf.getvalue())


def sha256(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        h.update(f.read())
    return h.hexdigest()


def manifest_files(option_dir):
    out = []
    for dirpath, dirnames, files in os.walk(option_dir):
        dirnames[:] = sorted(x for x in dirnames if x != '__pycache__')
        for name in sorted(files):
            rel = os.path.relpath(os.path.join(dirpath, name), option_dir)
            if rel not in MANIFEST_SKIP and not name.endswith(('.pyc', '.kicad_prl', '.lck')):
                out.append(rel)
    return sorted(out)


def write_manifest(option_dir):
    lines = [f'{sha256(os.path.join(option_dir, rel))}  {rel}' for rel in manifest_files(option_dir)]
    with open(os.path.join(option_dir, 'outputs.sha256'), 'w', encoding='utf-8') as f:
        f.write('# Written by scripts/boards/build.sh. tests/test_boards.py fails if any listed file changes without a\n'
                '# rebuild: regenerate with build.sh, never edit these outputs by hand.\n')
        f.write('\n'.join(lines) + '\n')


if __name__ == '__main__':
    cmd, args = sys.argv[1], sys.argv[2:]
    {'bom': write_bom, 'gerbers': zip_gerbers, 'manifest': write_manifest}[cmd](*args)
