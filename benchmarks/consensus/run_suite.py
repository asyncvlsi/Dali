"""Serial native benchmark orchestration and file audits; no Python optimizer."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import platform
import statistics
import subprocess
import sys
import time

from audit import audit

FAMILIES = {
    'development': ['pci_bridge32_a_md1', 'fft_2_md2', 'edit_dist_1_md1'],
    'held_out': ['des_perf_a_md1'],
}
MODES = ['single', 'mixed']
DENSITIES = [.5, .65, .8]


def sha(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for chunk in iter(lambda: stream.read(1 << 20), b''):
            h.update(chunk)
    return h.hexdigest()


def save(path, value):
    path.write_text(json.dumps(value, indent=2) + '\n')


def native(args, output, timeout):
    """wait4 measures each actual C++ child, including failed attempts."""
    output.mkdir(parents=True, exist_ok=False)
    start = time.perf_counter()
    with (output / 'stdout.log').open('w') as log:
        child = subprocess.Popen(args, cwd=output, stdout=log,
                                 stderr=subprocess.STDOUT)
        expired = False
        while True:
            pid, status, usage = os.wait4(child.pid, os.WNOHANG)
            if pid:
                break
            if time.perf_counter() - start >= timeout:
                child.kill()
                pid, status, usage = os.wait4(child.pid, 0)
                expired = True
                break
            time.sleep(.005)
        child.returncode = os.waitstatus_to_exitcode(status)
    record = dict(args=args, returncode=child.returncode, timeout=expired,
                  wall_s=time.perf_counter() - start,
                  cpu_s=usage.ru_utime + usage.ru_stime,
                  peak_rss_bytes=usage.ru_maxrss *
                  (1 if sys.platform == 'darwin' else 1024))
    if (output / 'metrics.json').exists():
        record['metrics'] = json.loads((output / 'metrics.json').read_text())
    for suffix in ['source', 'final', 'failed']:
        p = output / ('placed.multi_region.' + suffix)
        if p.exists():
            record[suffix + '_sha256'] = sha(p)
    save(output / 'receipt.json', record)
    return record


def software(repo, binaries):
    paths = subprocess.check_output(
        ['git', 'ls-files'], cwd=repo, text=True).splitlines()
    selected = {repo / p for p in paths
                if p.endswith(('.h', '.cc', '.cpp', 'CMakeLists.txt'))}
    selected.update(p for p in (repo / 'benchmarks/consensus').rglob('*')
                    if p.is_file() and '__pycache__' not in p.parts)
    return dict(
        head=subprocess.check_output(['git', 'rev-parse', 'HEAD'],
                                     cwd=repo, text=True).strip(),
        source_sha256={str(p.relative_to(repo)): sha(p)
                       for p in sorted(selected)},
        binary_sha256={name: sha(binaries / name)
                       for name in ['dali', 'consensus_prepare_public']})


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', type=Path, required=True)
    parser.add_argument('--binaries', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--phase', choices=FAMILIES, required=True)
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[2]
    output = args.output.resolve()
    current = software(repo, args.binaries)
    if args.phase == 'development':
        output.mkdir(parents=True, exist_ok=False)
        save(output / 'freeze.json', dict(software=current,
             platform=platform.platform(), repetitions=3,
             optimizer_seconds=1, max_iterations=128,
             stagnant_checkpoints=4, densities=DENSITIES, modes=MODES,
             families=FAMILIES))
        diff = subprocess.check_output(['git', 'diff', '--binary', 'HEAD'],
                                       cwd=repo)
        (output / 'tracked-source.patch').write_bytes(diff)
        for relative in current['source_sha256']:
            p = output / 'source' / relative
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_bytes((repo / relative).read_bytes())
    else:
        frozen = json.loads((output / 'freeze.json').read_text())
        assert current == frozen['software'], 'software changed after freeze'
        assert json.loads((output / 'development/summary.json').read_text())['complete']
    phase = output / args.phase
    phase.mkdir(exist_ok=False)
    cases = []
    originals = {}
    for family in FAMILIES[args.phase]:
        source = args.source_root / family
        for p in [source / (family + '.lef'), source / (family + '.def'),
                  source / 'placement.constraints']:
            if p.exists():
                originals[str(p)] = sha(p)
        for mode in MODES:
            for density in DENSITIES:
                key = f'{family}-{mode}-d{round(100*density)}'
                case_dir = phase / 'inputs' / key
                cmd = [str(args.binaries / 'consensus_prepare_public'),
                       str(source / (family + '.lef')),
                       str(source / (family + '.def')), str(case_dir),
                       mode, str(density)]
                record = native(cmd, phase / 'preparation' / key, 120)
                assert all(sha(p) == h for p, h in originals.items())
                if record['returncode'] == 0:
                    record['derived_sha256'] = {
                        p.name: sha(p) for p in case_dir.iterdir() if p.is_file()}
                save(phase / 'preparation' / key / 'receipt.json', record)
                cases.append(dict(key=key, family=family, mode=mode,
                                  density=density, prepared=record['returncode'] == 0))
                print('prepare', key, record['returncode'], flush=True)
    save(phase / 'original_sha256.json', originals)
    runs = {}
    for rep in range(3):
        for index, case in enumerate(cases):
            if not case['prepared']:
                continue
            key = case['key']; case_dir = phase / 'inputs' / key
            arms = ['baseline', 'consensus']
            if (rep + index) % 2:
                arms.reverse()
            for arm in arms:
                run = phase / 'runs' / key / f'{arm}-r{rep}'
                cmd = [str(args.binaries / 'dali'),
                       '-lef', str(case_dir / 'cells.lef'),
                       '-def', str(case_dir / 'input.def'),
                       '-cell', str(case_dir / 'cells.cell'),
                       '-enable_multi_region_legalization',
                       '-disable_global_place', '-disable_io_place',
                       '-target_density', '1', '-well_legalization_mode', 'scavenge',
                       '-o', str(run / 'placed'),
                       '-metrics_file', str(run / 'metrics.json'),
                       '-log_file_name', str(run / 'dali.log'), '-disable_log_prefix']
                if arm == 'consensus':
                    cmd += ['-enable_gridded_consensus', '-gridded_consensus_time', '1',
                            '-gridded_consensus_iterations', '128',
                            '-gridded_consensus_stagnation', '4']
                record = native(cmd, run, 180)
                runs[(key, arm, rep)] = record
                print('run', key, arm, rep, record['returncode'],
                      round(record['wall_s'], 3), flush=True)
    # Audits run after native timing; repetition zero is the declared quality
    # sample. All repetitions retain their native metrics and complete outputs.
    for case in cases:
        key = case['key']
        if not case['prepared']:
            case['status'] = 'conversion_failed'
            continue
        records = [runs[(key, arm, rep)] for arm in ['baseline', 'consensus']
                   for rep in range(3)]
        if any(r['returncode'] != 0 for r in records):
            case['status'] = 'native_failed'
            continue
        if len({r['source_sha256'] for r in records}) != 1:
            case['status'] = 'unmatched_sources'
            continue
        try:
            family = case['family']
            case['audit'] = audit(
                phase / 'inputs' / key,
                phase / 'runs' / key / 'consensus-r0',
                args.source_root / family / (family + '.def'))
            baseline = runs[(key, 'baseline', 0)]['metrics']['stages']
            final = runs[(key, 'consensus', 0)]['metrics']['stages']
            assert baseline['final'] == final['multi_region.initialization']
            case['baseline_hpwl_um'] = baseline['final']
            case['consensus_hpwl_um'] = final['final']
            case['gain_percent'] = 100 * (baseline['final'] - final['final']) / baseline['final']
            case['timing'] = {}
            for arm in ['baseline', 'consensus']:
                values = [runs[(key, arm, rep)] for rep in range(3)]
                case['timing'][arm] = {
                    k: statistics.median(v[k] for v in values)
                    for k in ['wall_s', 'cpu_s', 'peak_rss_bytes']}
                case['timing'][arm]['samples_wall_s'] = [v['wall_s'] for v in values]
                if arm == 'consensus':
                    case['timing'][arm]['stage_medians_s'] = {
                        k: statistics.median(v['metrics']['stages'][k] for v in values)
                        for k in ['time.consensus.preparation_s', 'time.consensus.optimizer_s',
                                  'time.consensus.validation_s', 'time.consensus.wall_s']}
            case['status'] = 'PASS'
        except Exception as e:
            case['status'] = 'audit_failed'
            case['audit_error'] = repr(e)
        print('audit', key, case['status'], flush=True)
    assert current == software(repo, args.binaries), 'software changed during run'
    assert all(sha(p) == h for p, h in originals.items()), 'original changed'
    save(phase / 'summary.json', dict(complete=True, cases=cases,
                                    originals_unchanged=True, software_unchanged=True))
    return 0 if all(c['status'] == 'PASS' for c in cases) else 1


if __name__ == '__main__':
    sys.exit(main())
