# KCF Tools v0.14 Release Notes

현재 Tool 버전은 **v0.14**, CMake project version은 **0.14.0**이다.
v0.13 후속 개발의 Generic Remote Control / Multi-Host UI를 이어받고,
Framework 연동 기준을 **KCF v5.4**로 정합화한다.
이번 변경은 버전 메타데이터와 문서에 한정되며 제품 동작·Network 프로토콜·테스트 코드를 변경하지 않는다.
Commit/push/Tag/GitHub Release는 생성하지 않는다.

## Framework 연동과 기능

검증 소스는 `../kss_control_framework`의 dev 커밋
**`5ae507a8b866015360686463c1ce03905c4e39af`**
(`v5.4 network remote control validation and deployment policy`)다.
Framework CMake 버전과 Discovery `FrameworkVersion` 모두 **5.4.0**임을 확인했다.
커밋은 이번 검증 기준점이며 향후 HEAD가 계속 같다는 뜻은 아니다.

- Multi-Host Discovery / Host 선택과 metadata 기반 Remote Application / Element / Endpoint 탐색
- Remote Parameter Get / Set / Readback
- Remote Service Call / Response
- Remote Action Goal / Feedback / Result / Cancel, ACK와 Terminal State 구분
- Remote Topic Subscribe / Unsubscribe, Payload·Sequence 및 수신률 표시
- TypeDescriptor 기반 generic scalar/fixed-array 입력·출력과 Framework canonical codec 재사용
- 기존 Network Diagnostics의 Route·9개 Counter·Rate 및 양쪽 Gateway의 ACTIVE Route를 확인하는 Verified Route Graph 유지

Framework의 공개 RemoteControlClient, RemoteTopicClient, TypeDescriptor Query,
NetworkDiagnosticsClient를 사용한다. Tool은 Gateway/forwarder나 장치 제어 로직을 대신하지 않는다.
기존 Local GUI/API, Mock/stub 및 구형 Framework의 API 유무에 따른 Local/Discovery 빌드 경로는 유지한다.
사용법과 권한 설정은 [Remote Control](REMOTE_CONTROL.md), 빌드는 [README](../README.md)를 따른다.

## 이번 v0.14 자동 검증

기존 `/tmp/kt-remote`, `/tmp/kt-remote-mock` 빌드 디렉터리를 v0.14로 다시 configure/build했다.
두 구성 모두 Debug, Qt 6.9.3 Widgets/Network이며 CMake cache의 project version은 0.14.0이다.
Framework 연동 구성의 KCF_SOURCE_DIR은 `/home/kssvm/workspace/kcf/kss_control_framework`,
Mock/stub 구성은 빈 값이다. 새 clean build를 수행한 것으로 표현하지 않는다.

| 구성 / 실행 | 결과 | 로그 |
| --- | --- | --- |
| Framework v5.4 연동 configure/build | PASS | `/tmp/kt-v014-configure.log`, `/tmp/kt-v014-build.log` |
| Framework v5.4 연동 전체 CTest | 19/19 PASS, 51.05초 | `/tmp/kt-v014-tests.log` |
| Mock/stub configure/build | PASS | `/tmp/kt-v014-mock-configure.log`, `/tmp/kt-v014-mock-build.log` |
| Mock/stub 전체 CTest | 6/6 PASS, 1.47초 | `/tmp/kt-v014-mock-tests.log` |
| Remote Control Virtual LAN / Qt offscreen E2E | PASS | `/tmp/kt-v014-remote-e2e/results.json` |

두 구성의 CTest와 E2E는 이번 실행에서 최초 PASS했다. CTest는 격리된 Loopback/SHM에서
Local Parameter/Service/Topic, Network Backend/Diagnostics 및 Qt offscreen GUI/Graph 회귀를 포함한다.
Remote Control E2E는 별도 Virtual LAN namespace에서 실행했다. 이 결과는 과거 v0.13 결과와 별도다.

기존 `tests/remote_control_e2e.py`의 Virtual LAN + Qt offscreen 실행은 PASS했다.
실제 Parameter Set/Readback, Service Response, Action Feedback/Result/Cancel 및 다른 요청 병행,
두 Host의 동일 이름 Topic 분리, GUI 조작·수신률, Session 변경 거부, GUI 재설정과 진행 중 요청 Stop을 확인했다.
Timeout은 `execution_unknown=1`, `response_received=0`이었고 fixture Service 실행 증가는 정확히 1회였다.
로그: `/tmp/kt-v014-remote-e2e/results.json`.

Configure/build 로그는 `/tmp/kt-v014-configure.log`, `/tmp/kt-v014-build.log`,
`/tmp/kt-v014-mock-configure.log`, `/tmp/kt-v014-mock-build.log`에 있다.
`/tmp` 로그는 임시 개발 artifact이며 배포에 포함되지 않는다.

## Historical — v0.13 / Framework v5.3 물리 LAN 수동 검증

다음은 **2026-10-21 당시 Framework v5.3 / Tools v0.13 소스**의 사용자 확인 결과다.
Orange Pi 5+ ↔ Ubuntu VM Bridged LAN에서 실제 Qt GUI로 검증했다.
이번 v0.14 / Framework v5.4의 물리 LAN 재검증 결과가 아니다.

- Discovery/Metadata: PASS
- Topic Subscribe 및 실제 Payload 수신: PASS, 약 45.96Hz 관측
- Parameter Get/Set/Readback 및 Service Call/Response: PASS
- Action Goal ACK/Feedback/SUCCEEDED Result: PASS
- Action Cancel ACK/CANCELED Terminal/Result: PASS
- Cancel Result: `count=-1, enabled=false, gain=1.5`
- `execution_unknown=0`, `cancel_unknown=0` 확인

UFW의 Discovery UDP 37651 차단과 동일 Host ID Gateway 중복 실행 문제를 정리하고,
Action Cancel 시험에는 Orange Pi의 테스트용 10초 설정을 사용했다.
양쪽 프로그램 전체 재시작 후 연결과 Cancel을 확인한 결과이며, 전체 안정성 인증이 아니다.
자세한 기록은 [사용자 물리 LAN 검증](REMOTE_CONTROL.md#물리-lan-수동-검증--사용자-확인)을 따른다.
[v0.13 릴리스 노트](V0_13_RELEASE_NOTES.md)와 이전 릴리스, 당시 PASS/FAIL·재실행 기록은 수정하지 않는다.

## 미지원 기능과 추가 검증

- 기존 Action의 자동 reattach 및 변경 요청의 자동 retry는 지원하지 않는다.
- Host ID allowlist는 암호학적 인증이나 통신 암호화를 제공하지 않는다.
- 물리 LAN에서 Gateway 단독 재시작 후 자동 재발견·복구, Tool 단독 재시작 및 일시적 LAN 단절 복구는 추가 검증이 필요하다.
- 실제 다중 Gateway 동시 제어와 장시간 재연결 안정성은 추가 검증이 필요하다.
- 이번 v0.14의 물리 LAN 및 실제 데스크톱 GUI 수동 검증은 수행하지 않았다.
- 구형 Framework 구성의 빌드 경로는 변경하지 않았으나 이번 작업에서 구형 Framework를 재빌드·재검증하지 않았다.
- Virtual LAN/Qt offscreen 결과를 위 미검증 항목의 PASS로 대신하지 않는다.
- Graph는 수동 Refresh이며 10초 freshness를 재생성 시 검사한다. 표시 후 자동 만료되지 않는다.
- truncated Snapshot은 부분 결과다. 이전 간헐적 실패의 원인 미확정 이력은 후속 PASS로 해결됐다고 단정하지 않는다.
