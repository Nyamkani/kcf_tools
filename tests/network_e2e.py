#!/usr/bin/env python3
"""Opt-in Virtual LAN test. Usage: network_e2e.py FRAMEWORK TOOL_BUILD OUTPUT.
Reuses Framework Lab/Process; namespace failure is SKIP (exit 77), not PASS.
Physical LAN remains untested. No product sources or host network are changed.
"""
import importlib.util
import os
from pathlib import Path
import signal
import subprocess
import sys
import time

# Import the external fixture without writing generated files into its repository.
sys.dont_write_bytecode = True


def run(framework, build, output):
    spec = importlib.util.spec_from_file_location('virtual_lan', framework/'tests/network_virtual/virtual_lan.py')
    lan = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(lan)
    lab = lan.Lab(build/'kcf_tool_network_e2e_fixture', output)
    def observer():
        binary = lab.binary
        lab.binary = str(build/'kcf_tool_network_e2e_observer')
        try:
            return lan.Process(lab, 1, 'observer')
        finally:
            lab.binary = binary
    def wait_snapshot(predicate, message):
        def check():
            value = tool.ask()
            return value if predicate(value) else None
        return lan.until(check, message, timeout=20)
    try:
        lab.setup()
        owner = lan.Process(lab, 1, 'owner')
        ga = lan.Process(lab, 1, 'gateway')
        gb = lan.Process(lab, 2, 'gateway')
        lan.until(lambda: gb.ask()['proxies'] == 1, 'remote proxy startup')
        sub = lan.Process(lab, 2, 'subscriber')
        tool = observer()
        initial = wait_snapshot(lambda v: v['online'] == 2 and v['edges'] == 1 and v['measured'], 'measured two-sided route')
        start = time.monotonic(); first = owner.ask()['published']
        time.sleep(3)
        last = owner.ask()['published']; elapsed = time.monotonic()-start
        result = wait_snapshot(lambda v: v['edges'] == 1 and v['measured'] and v['sent_count'] > initial['sent_count'] and v['receive_count'] > initial['receive_count'] and v['proxy_count'] > initial['proxy_count'], 'counters increase')
        result['publisher_hz'] = (last-first)/elapsed
        lan.require(150 <= result['publisher_hz'] <= 230, 'publisher measured rate')
        for key in ('send', 'receive', 'proxy'):
            lan.require(25 <= result[key] <= 65, f'{key} measured rate tolerance: {result[key]}')
        lan.require(75 <= result['skipped'] <= 220, 'skipped samples/s')
        lan.require(sub.ask()['callbacks'] > 0, 'application received actual payload')
        lab.record('real_backend_rates_graph', details=result)
        lab.record('freshness_manual_refresh', details=tool.ask('FRESHNESS'))
        # Stop the listener process without stopping discovery first: exercise bounded Query failure.
        os.kill(gb.proc.pid, signal.SIGSTOP)
        try:
            failed = wait_snapshot(lambda v: v['edges'] == 0 and v['timeouts'] > 0, 'failed query clears edge')
            lab.record('query_failure', details=failed)
        finally:
            os.kill(gb.proc.pid, signal.SIGCONT)
        wait_snapshot(lambda v: v['edges'] == 1, 'query recovery')
        sub.stop(); gb.stop(kill=True)
        lost = wait_snapshot(lambda v: v['edges'] == 0 and v['online'] < 2, 'LOST clears route')
        lab.record('gateway_lost', details=lost)
        gb = lan.Process(lab, 2, 'gateway')
        lan.until(lambda: gb.ask()['proxies'] == 1, 'restart proxy startup')
        sub = lan.Process(lab, 2, 'subscriber')
        restarted = wait_snapshot(lambda v: v['edges'] == 1 and v['session'] != result['session'] and v['measured'], 'new session route')
        lan.require(sub.ask()['callbacks'] > 0, 'restart subscriber payload')
        lab.record('gateway_restart_session', details=restarted)
        gb.ask('DISABLE')
        disabled = wait_snapshot(lambda v: v['edges'] == 0 and v['online'] == 2 and v['unsupported'] > 0, 'disabled diagnostics clears route')
        lab.record('diagnostics_disabled', details=disabled)
        lab.record('overall', details='Virtual LAN + independent Tool Observer + Qt offscreen PASS; physical LAN unverified')
    except Exception as exc:
        lab.record('overall', 'FAIL', str(exc))
        raise
    finally:
        lab.cleanup()


if __name__ == '__main__':
    if len(sys.argv) not in (4, 5):
        sys.exit(__doc__)
    internal = sys.argv[1] == '--run'
    framework, build, output = [Path(p).resolve() for p in sys.argv[2 if internal else 1:]]
    if not internal:
        preflight = subprocess.run(['unshare', '--user', '--map-root-user', '--net', '--ipc', '--mount', 'true'], capture_output=True)
        if preflight.returncode:
            print('UNVERIFIED: Virtual LAN namespace permission unavailable:', preflight.stderr.decode())
            sys.exit(77)
        env = dict(os.environ, QT_QPA_PLATFORM='offscreen')
        sys.exit(subprocess.call(['unshare', '--user', '--map-root-user', '--net', '--ipc', '--mount', '--pid', '--fork', '--kill-child=KILL', '--mount-proc', sys.executable, str(Path(__file__).resolve()), '--run', str(framework), str(build), str(output)], env=env))
    run(framework, build, output)
