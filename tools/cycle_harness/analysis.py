"""Statistics over harness runs: the cycle's shape, and where population and elite change comes from."""
import csv
import statistics
from collections import Counter, defaultdict
from pathlib import Path

PHASES = ('prosperity', 'strain', 'fracture')
PROSPERITY, STRAIN, FRACTURE = 0, 1, 2
CRISES = ('revolt', 'civil_war', 'coup')
COMMONER_FLOWS = (('births', 1), ('natural_deaths', -1), ('overshoot_deaths', -1), ('crisis_deaths', -1), ('war_deaths', -1))
ELITE_FLOWS = (('elite_births', 1), ('promotions', 1), ('demotions', -1), ('attrition', -1),
               ('elite_crisis_deaths', -1), ('elite_war_deaths', -1))
INTS = {'year', 'country', 'generation', 'locations', 'occupied', 'phase', 'in_lull',
        'revolt', 'revolt_new', 'civil_war', 'civil_war_new', 'coup', 'coup_new'}


def load(run_dir):
    """Each country's life (seed, country, generation) as its yearly rows."""
    run_dir = Path(run_dir)
    lives = defaultdict(list)
    for path in sorted(run_dir.glob('s*.csv')):
        seed = int(path.stem[1:])
        for raw in csv.DictReader(open(path)):
            row = {k: int(v) if k in INTS else float(v) for k, v in raw.items()}
            row['density'] = row['commoners'] / row['capacity']
            row['seed'] = seed
            lives[(seed, row['country'], row['generation'])].append(row)
    if not lives:
        raise SystemExit(f'no runs in {run_dir}')
    return list(lives.values())


def median(xs, digits=2):
    return round(statistics.median(xs), digits) if xs else None


def quantiles(xs):
    if len(xs) < 4:
        return '-'
    q = statistics.quantiles(xs, n=4)
    return f'{q[0]:.2f}/{statistics.median(xs):.2f}/{q[2]:.2f}'


def cycles(lives):
    """Prosperity entry to the next prosperity entry."""
    out = []
    for rows in lives:
        starts = [i for i in range(1, len(rows)) if rows[i]['phase'] == PROSPERITY and rows[i - 1]['phase'] == FRACTURE]
        for a, b in zip(starts, starts[1:]):
            seg = rows[a:b]
            e = [r['overproduction'] for r in seg]
            dens = [r['density'] for r in seg]
            phases = Counter(r['phase'] for r in seg)
            out.append({'years': b - a, 'phases': phases, 'e_peak': max(e), 'e_swing': max(e) / min(e),
                        'density_peak': max(dens), 'density_swing': max(dens) / min(dens)})
    return out


def fractures(lives):
    """Complete fracture spells: entered and left within the run."""
    out = []
    for rows in lives:
        i = 1
        while i < len(rows):
            if rows[i]['phase'] == FRACTURE and rows[i - 1]['phase'] != FRACTURE:
                j = i
                while j < len(rows) and rows[j]['phase'] == FRACTURE:
                    j += 1
                if j < len(rows):
                    before, spell = rows[i - 1], rows[i:j]
                    after25 = spell[24] if len(spell) >= 25 else None
                    out.append({
                        'years': j - i,
                        'commoners': spell[-1]['commoners'] / before['commoners'],
                        'commoners_25': after25['commoners'] / before['commoners'] if after25 else None,
                        'low': min(r['commoners'] for r in spell) / before['commoners'],
                        'elites': spell[-1]['elites'] / before['elites'],
                        'wars': sum(r['civil_war_new'] for r in spell),
                        'starts': Counter({c: sum(r[c + '_new'] for r in spell) for c in CRISES}),
                    })
                i = j
            else:
                i += 1
    return out


def flows_by_phase(lives):
    """Each flow as a share of the year's opening stock, by the phase the year opened in."""
    sums = defaultdict(lambda: defaultdict(float))
    for rows in lives:
        for prev, row in zip(rows, rows[1:]):
            s = sums[prev['phase']]
            s['commoners'] += prev['commoners']
            s['elites'] += prev['elites']
            s['commoner_change'] += row['commoners'] - prev['commoners']
            s['elite_change'] += row['elites'] - prev['elites']
            for name, _ in COMMONER_FLOWS + ELITE_FLOWS:
                s[name] += row[name]
            s['years'] += 1
    return sums


def crisis_rates(lives):
    """Crisis starts per country-century, by the phase they started in."""
    years = Counter()
    starts = defaultdict(Counter)
    for rows in lives:
        for prev, row in zip(rows, rows[1:]):
            years[prev['phase']] += 1
            for c in CRISES:
                starts[prev['phase']][c] += row[c + '_new']
    return years, starts


def ledger_residual(lives):
    """How far the ledger's flows miss the actual change; only whole-person rounding should remain."""
    com, eli = [], []
    for rows in lives:
        for prev, row in zip(rows, rows[1:]):
            flow_c = sum(sign * row[n] for n, sign in COMMONER_FLOWS)
            flow_e = sum(sign * row[n] for n, sign in ELITE_FLOWS)
            com.append(abs(prev['commoners'] + flow_c - row['commoners']) / max(prev['commoners'], 1))
            eli.append(abs(prev['elites'] + flow_e - row['elites']) / max(prev['elites'], 1))
    return statistics.mean(com), statistics.mean(eli)


def split(lives):
    count = Counter(r['phase'] for rows in lives for r in rows)
    total = sum(count.values())
    return [count[p] / total for p in range(3)]


def compact(lives, name):
    cy = cycles(lives)
    fr = fractures(lives)
    years, starts = crisis_rates(lives)
    s = split(lives)
    wars = Counter(min(f['wars'], 3) for f in fr)
    per_century = lambda phase: sum(starts[phase].values()) / max(years[phase], 1) * 100
    return (f'{name:16} P/S/F {s[0]:.2f}/{s[1]:.2f}/{s[2]:.2f}  period {median([c["years"] for c in cy])}'
            f' (P {median([c["phases"][0] for c in cy])} S {median([c["phases"][1] for c in cy])} F {median([c["phases"][2] for c in cy])})'
            f'  E swing {median([c["e_swing"] for c in cy])} peak {median([c["e_peak"] for c in cy])}'
            f'  pop swing {median([c["density_swing"] for c in cy])} peak {median([c["density_peak"] for c in cy])}'
            f'  fracture pop x{median([f["commoners"] for f in fr])} (25y x{median([f["commoners_25"] for f in fr if f["commoners_25"]])})'
            f'  wars/fracture 0:{wars[0]} 1:{wars[1]} 2:{wars[2]} 3+:{wars[3]}'
            f'  crises/century S {per_century(STRAIN):.1f} F {per_century(FRACTURE):.1f}'
            f'  fractures>=120y {len(stuck(lives, 120))}')


def pct(x):
    return f'{x * 100:+6.2f}'


def report(lives, name):
    lines = [compact(lives, name), '']
    cy = cycles(lives)
    fr = fractures(lives)
    lines.append(f'cycles {len(cy)}, complete fractures {len(fr)}, countries {len(lives)}')
    lines.append(f'  period quartiles {quantiles([c["years"] for c in cy])}')
    lines.append(f'  fracture years {quantiles([f["years"] for f in fr])}, longest {max((f["years"] for f in fr), default="-")}')
    lines.append(f'  fracture commoners end/start {quantiles([f["commoners"] for f in fr])}, lowest/start {quantiles([f["low"] for f in fr])}')
    lines.append(f'  fracture commoners after 25 years {quantiles([f["commoners_25"] for f in fr if f["commoners_25"]])}')
    lines.append(f'  fracture elites end/start {quantiles([f["elites"] for f in fr])}')
    lines.append('')

    sums = flows_by_phase(lives)
    header = f'{"% of stock per year":22}' + ''.join(f'{p:>12}' for p in PHASES)
    lines.append('commoners: ' + header)
    for flow, sign in COMMONER_FLOWS:
        lines.append(f'  {flow:31}' + ''.join(f'{pct(sign * sums[p][flow] / max(sums[p]["commoners"], 1)):>12}' for p in range(3)))
    lines.append(f'  {"net (observed)":31}' + ''.join(f'{pct(sums[p]["commoner_change"] / max(sums[p]["commoners"], 1)):>12}' for p in range(3)))
    lines.append('elites:    ' + header)
    for flow, sign in ELITE_FLOWS:
        lines.append(f'  {flow:31}' + ''.join(f'{pct(sign * sums[p][flow] / max(sums[p]["elites"], 1)):>12}' for p in range(3)))
    lines.append(f'  {"net (observed)":31}' + ''.join(f'{pct(sums[p]["elite_change"] / max(sums[p]["elites"], 1)):>12}' for p in range(3)))
    lines.append('')

    years, starts = crisis_rates(lives)
    lines.append('crisis starts per country-century: ' + ''.join(f'{p:>12}' for p in PHASES))
    for c in CRISES:
        lines.append(f'  {c:33}' + ''.join(f'{starts[p][c] / max(years[p], 1) * 100:12.1f}' for p in range(3)))
    lines.append('')
    com, eli = ledger_residual(lives)
    lines.append(f'ledger residual (rounding): commoners {com * 100:.4f}% elites {eli * 100:.4f}% of stock per year')
    return '\n'.join(lines)


def trace(lives, seed, country, start, end, every):
    rows = next((r for r in lives if r[0]['seed'] == seed and r[0]['country'] == country), None)
    if rows is None:
        raise SystemExit(f'no country {country} in seed {seed}')
    head = ('year ph  dens  wage     E  felt  Sc/t*     Se/t*     R/R*      Re   vio legit   sec armsh |'
            ' births  natd  over  cris   war   net |  ebir  prom  demo  attr  ecri  ewar   net | crises')
    out = [head]
    for prev, row in zip(rows, rows[1:]):
        if not (start <= row['year'] <= end) or row['year'] % every:
            continue
        c = prev['commoners'] or 1
        e = prev['elites'] or 1
        cf = [sign * row[n] / c * 100 for n, sign in COMMONER_FLOWS]
        ef = [sign * row[n] / e * 100 for n, sign in ELITE_FLOWS]
        crises = ''.join(ch.upper() if row[k + '_new'] else ch if row[k] else '.' for ch, k in zip('rwc', CRISES))
        out.append(f'{row["year"]:4} {row["phase"]:2} {row["density"]:5.2f} {row["wage"]:5.2f} {row["overproduction"]:5.2f}'
                   f' {row["felt_overproduction"]:5.2f}'
                   f' {row["commoner_satisfaction"]:4.2f}/{row["commoner_satisfaction_target"]:4.2f}'
                   f' {row["elite_satisfaction"]:4.2f}/{row["elite_satisfaction_target"]:4.2f}'
                   f' {row["radicalism"]:4.2f}/{row["radicalism_target"]:4.2f}'
                   f' {row["effective_radicalism"]:5.2f} {row["violence"]:5.2f} {row["legitimacy"]:5.2f}'
                   f' {row["security"]:5.2f} {row["army_shortfall"]:5.2f} |'
                   + ''.join(f'{x:6.2f}' for x in cf) + f'{(row["commoners"] - prev["commoners"]) / c * 100:6.2f} |'
                   + ''.join(f'{x:6.2f}' for x in ef) + f'{(row["elites"] - prev["elites"]) / e * 100:6.2f} | {crises}')
    return '\n'.join(out)


def stuck(lives, years):
    """Fracture spells at least this long, still open or not, as (seed, country, start, length, locations)."""
    out = []
    for rows in lives:
        start = None
        for r in rows + [None]:
            if r is not None and r['phase'] == FRACTURE:
                start = r if start is None else start
                continue
            if start is not None:
                length = (r['year'] if r else rows[-1]['year'] + 1) - start['year']
                if length >= years:
                    out.append((start['seed'], start['country'], start['year'], length, start['locations'], r is None))
            start = None
    return sorted(out, key=lambda x: -x[3])

