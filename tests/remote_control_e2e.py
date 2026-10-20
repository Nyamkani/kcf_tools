#!/usr/bin/env python3
"""Run under the same isolated user/net/mount/PID namespace as network_e2e.py.
Usage: python3 remote_control_e2e.py FRAMEWORK TOOL_BUILD OUTPUT
Requires Qt offscreen. No host network changes; physical LAN is not covered.
"""
import importlib.util
import os
import time
import sys
from pathlib import Path
sys.dont_write_bytecode = True
framework, build, output = map(Path, sys.argv[1:])
# GUI Apply must not persist a fixture Host ID in the user's settings.
os.environ['XDG_CONFIG_HOME'] = str(output.resolve() / 'config')
spec = importlib.util.spec_from_file_location('lan', framework/'tests/network_virtual/virtual_lan.py')
lan = importlib.util.module_from_spec(spec)
spec.loader.exec_module(lan)
lab = lan.Lab(build/'kcf_tool_network_e2e_fixture', output)
try:
    lab.setup()
    owners = [lan.Process(lab, h, 'owner') for h in (1, 2)]
    shared = [lan.Process(lab, h, 'shared') for h in (1, 2)]
    gateways = [lan.Process(lab, h, 'gateway') for h in (1, 2)]
    fixture = lab.binary
    lab.binary = str(build.resolve()/'kcf_tool_remote_workspace')
    tool = lan.Process(lab, 1, 'observer')
    lab.binary = fixture
    lab.record('generic_control_topic_ui', details=tool.ask('CHECK'))
    gateways[0].ask('STOP')
    gateways[0].ask('START')
    lab.record('session_change', details=tool.ask('STALE'))
    lab.record('gui_reconfigure', details=tool.ask('RECONFIGURE'))
    calls_before = owners[0].ask('STATS')['service_calls']
    try:
        lab.record('timeout_outcome', details=tool.ask('TIMEOUT'))
    finally:
        time.sleep(0.7)  # Let the fixture's 600 ms callback complete.
        calls = owners[0].ask('STATS')['service_calls'] - calls_before
        lab.record('timeout_service_executions', details=calls)
        lan.require(0 <= calls <= 1, 'Timeout must not trigger automatic Service retry')
    lab.record('inflight_stop', details=tool.ask('STOP'))
    tool.stop()
    lan.require(tool.proc.returncode == 0, 'GUI close cleanup')
    lab.record('overall', details='Virtual LAN; physical LAN unverified')
except Exception as exc:
    lab.record('overall', 'FAIL', str(exc))
    raise
finally:
    lab.cleanup()
