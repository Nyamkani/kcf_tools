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
    # Use actual local registrations, Discovery and Qt widgets. Only Metadata
    # responses are dropped on host 2's private veth; announces/data keep flowing.
    lab.record('snapshot_ready', details=tool.ask('SNAPSHOT_SELECT'))
    lab.record('snapshot_subscription', details=tool.ask('SNAPSHOT_SUBSCRIBE'))
    lab.inside(2, 'tc', 'qdisc', 'add', 'dev', 'lan0', 'root', 'handle', '1:',
               'prio', 'bands', '3', 'priomap', *(['0']*16))
    try:
        lab.inside(2, 'tc', 'qdisc', 'add', 'dev', 'lan0', 'parent', '1:2',
                   'handle', '20:', 'netem', 'loss', '100%')
        # IPv4 without options: UDP payload offset 28; KCFD kind at offset 34.
        # Match magic and METADATA_RESPONSE, not the announce or topic streams.
        lab.inside(2, 'tc', 'filter', 'add', 'dev', 'lan0', 'protocol', 'ip',
                   'parent', '1:', 'prio', '1', 'u32',
                   'match', 'ip', 'protocol', '17', '0xff',
                   'match', 'u32', '0x4b434644', '0xffffffff', 'at', '28',
                   'match', 'u16', '0x0003', '0xffff', 'at', '34', 'flowid', '1:2')
        shared[1].ask('ADD')
        lab.record('snapshot_stale_gates_handle', details=tool.ask('SNAPSHOT_UPDATING'))
    finally:
        lab.inside(2, 'tc', 'qdisc', 'del', 'dev', 'lan0', 'root')
    lab.record('snapshot_complete_same_handle', details=tool.ask('SNAPSHOT_COMPLETE'))
    shared[1].ask('RECREATE')
    lab.record('snapshot_registration_changed', details=tool.ask('SNAPSHOT_REPLACED'))
    tool.ask('SNAPSHOT_SELECT')
    shared[1].ask('REMOVE')
    lab.record('snapshot_endpoint_removed', details=tool.ask('SNAPSHOT_REMOVED'))
    shared[1].ask('RECREATE')
    tool.ask('SNAPSHOT_SELECT')
    gateways[1].ask('STOP')
    lab.record('snapshot_lost', details=tool.ask('SNAPSHOT_LOST'))
    gateways[1].ask('START')
    tool.ask('SNAPSHOT_SELECT')
    gateways[1].ask('STOP'); gateways[1].ask('START')
    lab.record('snapshot_session_changed', details=tool.ask('SNAPSHOT_SESSION'))
    shared[1].ask('REMOVE'); owners[1].stop()
    lab.record('snapshot_valid_empty', details=tool.ask('SNAPSHOT_EMPTY'))
    lab.record('inflight_stop', details=tool.ask('STOP'))
    tool.stop()
    lan.require(tool.proc.returncode == 0, 'GUI close cleanup')
    lab.record('overall', details='Virtual LAN; physical LAN unverified')
except Exception as exc:
    lab.record('overall', 'FAIL', str(exc))
    raise
finally:
    lab.cleanup()
