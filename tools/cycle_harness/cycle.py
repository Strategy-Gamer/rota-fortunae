"""Secular-cycle tuning harness: runs the sim natively over the real map, many seeds at once.

  py tools/cycle_harness/cycle.py run NAME [--seeds 4] [--years 600] [--countries 12] [--no-crises]
                                    [--hold-radicalism X@A-B] [--start-density X] [--growth G] [KNOB=VALUE ...]
  py tools/cycle_harness/cycle.py summary NAME            full report for one run
  py tools/cycle_harness/cycle.py compare NAME [NAME ...] one line per run
  py tools/cycle_harness/cycle.py trace NAME [--seed 0] [--country 0] [--start 1] [--end 9999] [--every 1]
  py tools/cycle_harness/cycle.py stuck NAME [--years 120] fracture spells at least that long, to trace
  py tools/cycle_harness/cycle.py knobs                    every knob with its value in the source

Every `const Fixed64` in the cycle and civil-war sources is a knob under its own name, and each
crisis's row of CRISIS_CFG is CRISIS_<TYPE>_BASE, _MIN_YEARS, _MAX_YEARS and _COOLDOWN. Values are
decimals in the constant's own units. Knobs exist only in the harness build; the game is untouched.
The build regenerates itself whenever a source changes. Runs land in tools/cycle_harness/runs/.

Test interventions: --hold-radicalism X@A-B sets every country's radicalism to X at the start of each
year from A to B, --start-density X starts every owned location at X commoners per head of capacity
instead of the scenario's spread, and --growth G grows every location's capacity by G a year.
"""
import argparse
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

import analysis

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[1]
BUILD = HERE / 'build'
RUNS = HERE / 'runs'
EXE = BUILD / ('harness.exe' if os.name == 'nt' else 'harness')
MAP_PNG = REPO / 'project/MapData/provinces.png'
LOCATIONS = REPO / 'project/MapData/provinces_locations.txt'
STRAITS = REPO / 'project/MapData/provinces_straits.txt'
KNOBBED = ['src/economy/secular_cycle_system.cpp', 'src/countries/civil_war.cpp']
NOT_KNOBS = {'ZERO', 'ONE'}


def knobbed_source(text, path):
    """The source with each constant read through knobs::fixed/whole."""
    out, count = re.subn(r'^const Fixed64 (\w+) = (.+);',
                         lambda m: m.group(0) if m.group(1) in NOT_KNOBS
                         else f'const Fixed64 {m.group(1)} = knobs::fixed("{m.group(1)}", {m.group(2)});',
                         text, flags=re.M)
    out, rows = re.subn(r'\{ (milli\(\d+\)),\s*(\d+),\s*(\d+),\s*(\d+) \}, // (\w[\w ]*)',
                        lambda m: (lambda n: f'{{ knobs::fixed("CRISIS_{n}_BASE", {m.group(1)}), '
                                             f'knobs::whole("CRISIS_{n}_MIN_YEARS", {m.group(2)}), '
                                             f'knobs::whole("CRISIS_{n}_MAX_YEARS", {m.group(3)}), '
                                             f'knobs::whole("CRISIS_{n}_COOLDOWN", {m.group(4)}) }}, // {m.group(5)}')
                        (m.group(5).strip().upper().replace(' ', '_')), out)
    if count == 0:
        sys.exit(f'{path}: no constants found to make knobs of; has its style changed?')
    return '#include "knobs.h"\n' + out


def compiler():
    if os.environ.get('CXX'):
        return os.environ['CXX']
    msys = Path('C:/msys64/mingw64/bin/g++.exe')
    return str(msys) if msys.exists() else 'g++'


SIM_DIRS = ['core', 'countries', 'economy', 'map', 'queries', 'scenario', 'time', 'utility']  # everything but the Godot glue


def sources():
    return sorted(p for d in SIM_DIRS for p in (REPO / 'src' / d).glob('*.cpp'))


def build():
    inputs = sources() + [p for d in SIM_DIRS for p in (REPO / 'src' / d).glob('*.h')] + [HERE / 'harness.cpp', HERE / 'knobs.cpp', HERE / 'knobs.h', Path(__file__)]
    if EXE.exists() and max(p.stat().st_mtime for p in inputs) < EXE.stat().st_mtime:
        return
    BUILD.mkdir(exist_ok=True)
    generated = []
    for rel in KNOBBED:
        dst = BUILD / Path(rel).name
        dst.write_text(knobbed_source((REPO / rel).read_text(encoding='utf-8'), rel), encoding='utf-8')
        generated.append(dst)
    plain = [p for p in sources() if p.relative_to(REPO).as_posix() not in KNOBBED]
    cmd = [compiler(), '-std=c++17', '-O2', '-static', f'-I{REPO / "src"}', f'-I{HERE}',
           *map(str, plain), *map(str, generated), str(HERE / 'harness.cpp'), str(HERE / 'knobs.cpp'), '-o', str(EXE)]
    print('building harness...', flush=True)
    env = dict(os.environ)
    env['PATH'] = str(Path(cmd[0]).parent) + os.pathsep + env['PATH']  # g++ finds its own tools
    if subprocess.run(cmd, env=env).returncode != 0:
        sys.exit('harness build failed')


def map_rgb():
    """The province map as raw RGB8, as Godot hands it to the map builder."""
    rgb = BUILD / 'provinces.rgb'
    size = BUILD / 'provinces.size'
    if not rgb.exists() or rgb.stat().st_mtime < MAP_PNG.stat().st_mtime:
        from PIL import Image
        image = Image.open(MAP_PNG).convert('RGB')
        rgb.write_bytes(image.tobytes())
        size.write_text(f'{image.width} {image.height}')
    return rgb, size.read_text().split()


def knob_table():
    out = subprocess.run([str(EXE), '--knobs'], capture_output=True, text=True, check=True).stdout
    return {line.split()[0]: line.split()[1] for line in out.splitlines()}


def cmd_run(args):
    build()
    known = knob_table()
    env = dict(os.environ)
    overrides = {}
    for item in args.knobs:
        name, _, value = item.partition('=')
        if name not in known:
            close = [k for k in known if name.lower() in k.lower()]
            sys.exit(f'unknown knob {name}' + (f' (did you mean {", ".join(close)}?)' if close else ''))
        overrides[name] = value
    if args.no_crises:  # wins over any crisis knob, so one set of knobs serves both runs
        overrides.update({k: '0' for k in known if k.startswith('CRISIS_') and k.endswith('_BASE')})
    env.update(overrides)

    rgb, (width, height) = map_rgb()
    out = RUNS / args.name
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)
    controls = ([f'radicalism={args.hold_radicalism}'] if args.hold_radicalism else []) +         ([f'start={args.start_density}'] if args.start_density else []) +         ([f'growth={args.growth}'] if args.growth else [])
    (out / 'knobs.txt').write_text(''.join(f'{k}={v}\n' for k, v in sorted(overrides.items())) + ''.join(f'{c}\n' for c in controls))
    procs = [subprocess.Popen([str(EXE), str(rgb), width, height, str(LOCATIONS), str(STRAITS), str(seed),
                               str(args.countries), str(args.years), str(out / f's{seed}.csv'), *controls],
                              env=env, stdout=subprocess.PIPE, text=True)
             for seed in range(args.seeds)]
    hashes = []
    for seed, p in enumerate(procs):
        stdout, _ = p.communicate()
        if p.returncode != 0:
            sys.exit(f'seed {seed} failed')
        hashes.append(f's{seed} {stdout.strip()}')
    (out / 'hashes.txt').write_text('\n'.join(hashes) + '\n')
    print(analysis.compact(analysis.load(out), args.name))


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest='command', required=True)
    run = sub.add_parser('run')
    run.add_argument('name')
    run.add_argument('--seeds', type=int, default=4)
    run.add_argument('--years', type=int, default=600)
    run.add_argument('--countries', type=int, default=12)
    run.add_argument('--no-crises', action='store_true')
    run.add_argument('--hold-radicalism', metavar='X@A-B')
    run.add_argument('--start-density', metavar='X')
    run.add_argument('--growth', metavar='G')
    run.add_argument('knobs', nargs='*', metavar='KNOB=VALUE')
    summary = sub.add_parser('summary')
    summary.add_argument('name')
    compare = sub.add_parser('compare')
    compare.add_argument('names', nargs='+')
    trace = sub.add_parser('trace')
    trace.add_argument('name')
    trace.add_argument('--seed', type=int, default=0)
    trace.add_argument('--country', type=int, default=0)
    trace.add_argument('--start', type=int, default=1)
    trace.add_argument('--end', type=int, default=10**9)
    trace.add_argument('--every', type=int, default=1)
    stuck = sub.add_parser('stuck')
    stuck.add_argument('name')
    stuck.add_argument('--years', type=int, default=120)
    sub.add_parser('knobs')
    args = parser.parse_args()

    if args.command == 'run':
        cmd_run(args)
    elif args.command == 'summary':
        print(analysis.report(analysis.load(RUNS / args.name), args.name))
    elif args.command == 'compare':
        for name in args.names:
            print(analysis.compact(analysis.load(RUNS / name), name))
    elif args.command == 'trace':
        print(analysis.trace(analysis.load(RUNS / args.name), args.seed, args.country, args.start, args.end, args.every))
    elif args.command == 'stuck':
        for seed, country, start, length, locations, still_open in analysis.stuck(analysis.load(RUNS / args.name), args.years):
            print(f'seed {seed} country {country}: fracture from {start} for {length} years{" (still open)" if still_open else ""}, {locations} locations')
    elif args.command == 'knobs':
        build()
        for name, value in knob_table().items():
            print(f'{name:28} {value}')


if __name__ == '__main__':
    main()
