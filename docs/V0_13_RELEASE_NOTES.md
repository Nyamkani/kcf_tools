# KCF Tools v0.13 Release Notes

현재 Tool 버전은 **v0.13**, CMake project version은 **0.13.0**이다.
초기 Network Diagnostics 단계 이후 Generic Remote Control / Multi-Host UI가 추가되었다.
이번 문서 최신화는 기존 구현·검증 기록과 사용자 물리 LAN 확인 결과를 정리하며,
빌드·테스트를 재실행하거나 새 버전·Tag·Release를 생성하지 않는다. Commit/push도 수행하지 않는다.

## 현재 구현과 Framework 호환 기준

- **KCF Framework v5.3**, Framework `dev` 기준점: **`9e9cc1b`**
- KCF Tools `main` 기준점: **`e332438`**

위 커밋 ID는 사용자 지정 당시 기준점이며, 현재 HEAD가 계속 동일하다는 의미가 아니다.
과거 F-ND/KT-ND의 미커밋 working tree 검증을 위 커밋의 재검증 결과로 소급하지 않는다.
Remote 기능에는 Framework의 공개 `RemoteControlClient`, `RemoteTopicClient`,
TypeDescriptor Query 및 `NetworkDiagnosticsClient` API가 포함된 구성이 필요하다.

현재 제공하는 기능:

- Multi-Host Discovery 및 Host 선택, metadata 기반 Remote Application / Element / Endpoint 탐색
- Remote Parameter Get / Set / Readback
- Remote Service Call / Response
- Remote Action Goal / Feedback / Result / Cancel: Goal ACK, Cancel ACK, Terminal State를 별도로 표시
- Remote Topic Subscribe / Unsubscribe, Payload·Sequence 및 수신률 표시
- TypeDescriptor 기반 generic scalar/fixed-array 입력·출력 및 Framework canonical codec 재사용
- 기존 Network Diagnostics의 Route·9개 Counter·Rate와 양쪽 Gateway에서 확인된 ACTIVE Route Graph 유지

기존 Local 탐색·Topic Echo·Parameter Editor·Service Call은 유지하며 Remote Host 화면과 구분한다.
SystemStatus는 Local 전용이다. Tool은 Framework 공개 API를 사용하는 Client이며,
Gateway/forwarder나 Application의 장치 제어 로직을 대신하지 않는다.
Remote Client API가 없는 구성은 제공되는 API 범위의 관측 기능을 유지한다.
Diagnostics API가 없는 구형 Network Framework는 Discovery-only, Network API도 없으면 Local-only,
Framework 미지정은 Mock/stub 구성이다.

## 2026-10-21 물리 LAN 수동 검증 — 사용자 확인

출처: [Remote Control 물리 LAN 기록](REMOTE_CONTROL.md#물리-lan-수동-검증--사용자-확인).
Framework v5.3 / KCF Tools v0.13으로 **Orange Pi 5+ ↔ Ubuntu VM Bridged LAN**에서
**실제 Qt GUI**를 사용한 사용자 확인 결과다. 자동 테스트 또는 이번 문서 작업의 재실행 결과가 아니다.

| 항목 | 사용자 확인 결과 |
| --- | --- |
| Discovery / Metadata | PASS |
| Remote Topic Subscribe 및 실제 Payload 수신 | PASS, 약 45.96Hz 관측 |
| Parameter Get / Set / Readback | PASS |
| Service Call / Response | PASS |
| Action Goal ACK / Feedback / SUCCEEDED Result | PASS |
| Action Cancel ACK / CANCELED Terminal 및 Result | PASS |

Cancel Result는 `count=-1, enabled=false, gain=1.5`였고,
`execution_unknown=0`, `cancel_unknown=0`을 확인했다.
45.96Hz는 해당 환경의 관측값이며 보장 전송률이나 Application 처리율이 아니다.

VM UFW의 Discovery UDP `37651` 차단으로 Peer LOST가 발생했고,
동일 Host ID의 Gateway 중복 실행으로 Endpoint 발견 문제가 있었다.
Action 예제의 기본 2초 완료 시간 때문에 Cancel 수동 시험이 어려워 Orange Pi에서
테스트용 10초 설정을 사용했다. Tool의 Timeout 기본값 변경을 의미하지 않는다.
문제를 정리하고 **양쪽 프로그램 전체 재시작 후** 정상 연결과 Cancel을 검증했다.
이는 한쪽만 재시작한 복구 검증이나 전체 안정성 인증이 아니다.

## 현재 미지원 기능과 추가 검증 항목

- 기존 Action의 자동 reattach는 지원하지 않는다.
- 변경 요청의 자동 재시도는 지원하지 않는다. Timeout/연결 종료를 실제 하드웨어 정지로 해석하지 않는다.
- 물리 LAN에서 Gateway만 재시작한 자동 재발견·복구와 Tool만 재시작한 복구는 추가 검증이 필요하다.
- 일시적인 LAN 단절 후 복구 및 실제 다중 Gateway 동시 제어도 추가 검증이 필요하다.
- Host ID allowlist는 암호학적 인증이나 통신 암호화를 제공하지 않는다.
- Graph는 수동 Refresh이며, 10초 freshness는 재생성 시 검사한다. 표시 후 자동 만료되지 않는다.
- truncated Diagnostics Snapshot은 부분 결과이며 전체 Route 목록을 보장하지 않는다.

Virtual LAN 자동 검증 결과를 위 물리 LAN 미검증 항목의 PASS로 대신하지 않는다.
후속 Remote 통합의 자동 검증과 초기 GUI 대기 실패·100ms Timeout probe 실패 및 재실행 이력은
[Remote 통합 당시 자동 검증 기록](REMOTE_CONTROL.md#이번-작업의-검증-결과)에 별도로 보존되어 있다.
최신 사용법은 [README](../README.md), 상세 계약·권한 설정은 [Remote Control](REMOTE_CONTROL.md)을 따른다.

## Historical — 초기 F-ND / KT-ND Diagnostics 단계

아래는 KT-ND4/KT-ND5B까지의 원래 기록이다. 아래의 “현재/이번”, 미커밋 Framework 기준,
Remote 쓰기·Topic 수신 미지원 및 물리 LAN 미실행 설명은 **당시 상태**를 뜻한다.
후속 구현·수동 검증으로 당시 PASS/FAIL·재실행·미검증 이력을 바꾸지 않는다.
현재 구현 및 미지원 범위는 위 절을 따른다.

현재 Tool 버전은 **v0.13**, CMake project version은 **0.13.0**이다.
기준 Framework는 **kss_control_framework dev v5.3.0, F-ND5B 완료 미커밋 working tree**다.
이 working tree를 대표하는 공개 commit SHA/tag는 기재하지 않는다.
KT-ND4에서 버전·문서를 정리했고, 이후 KT-ND5A의 테스트 정합화·검증 결과를 KT-ND5B에서 반영했다.
아래 PASS는 출처별 기존 실행 기록이며 KT-ND5B 문서 정정에서는 build/test/configure를 재실행하지 않았다.
KT-ND4의 CMake configure 검사 이력은 유지하며, KT-ND5B에서는 문서 검사를 수행한다.
이번 작업은 commit/push/tag/GitHub Release를 생성하지 않았다.

### 기능과 호환성

기존 Local 기능과 Remote Discovery를 유지하면서 다음 read-only 관측을 추가한다.

- KT-ND1A: 기존 PeerDiscovery/worker에 공개 NetworkDiagnosticsClient 연결.
  Gateway별 Route 전체 identity/session과 9개 Counter, 정상 빈 결과/오류/Timeout/미지원/truncated 상태 보존.
- KT-ND1B: 성공 Counter Delta와 steady_clock 경과 시간으로 Send/Receive/Proxy Publish Hz,
  Local Skipped/s 계산. 최소 0.5초, 최초 Measuring, 오류·세대 변경·Counter 감소 시 무효화.
- KT-ND1C: 기존 Remote Topic Detail에 개별 Route 상태·Counter·Rate 표시.
- KT-ND1D: 양쪽 Gateway가 확인한 최신 ACTIVE Route만 청록색 방향 Edge로 표시.
  전체 Endpoint identity, session, route_id, 송수신 역할 및 실제 metadata registration을 검사한다.
- KT-ND2: 두 Gateway의 실제 Topic 전송 → 독립 Tool Observer → Backend Snapshot → Qt offscreen Graph 통합 검증.
- KT-ND3: 구형 Explorer 반복, 변경 범위 및 주요 회귀 검토. 외부 Framework에 생성된 Python 캐시를
  제거하고 테스트 스크립트의 캐시 생성을 방지했다. 제품 기능/Timeout은 수정하지 않았다.

Diagnostics API가 없는 구형 Network Framework에서는 기존 Discovery-only 관측을 유지한다.
Network API도 없는 Framework는 Local-only, Framework 미지정은 Mock/stub 구성이다.
Framework 공개 API만 사용하며 Gateway/forwarder, Topic payload 수신, Remote Set/Call/Goal/Cancel UI를 추가하지 않는다.
Proxy Publish Hz는 Gateway Local SHM 발행률이며 Application Subscriber 처리율이 아니다.

현재 계약: [Network Observation](NETWORK_OBSERVATION.md#v013-network-diagnostics-계약),
[Verified Route Graph](GRAPH.md#v013-verified-network-route-graph), [README](../README.md).

### 검증 기록

아래 표는 KT-ND2/ND3 당시 Diagnostics 적용 Framework working tree로 실행한 결과다.
당시 결과를 F-ND5B 버전·문서 정리 이후 소스의 새 실행 결과로 소급하지 않는다.

| 실행 | 결과 | 환경 / 출처 |
| --- | --- | --- |
| KT-ND3 현재 Framework + Tool | 19/19 PASS | 격리 Loopback/SHM, Qt offscreen; `/tmp/kt-nd3/tool-real.log` |
| KT-ND3 구형 Framework + Tool | 18/18 PASS | 별도 구형 소스, 격리 Loopback/SHM, Qt offscreen; `tool-legacy.log` |
| KT-ND3 Framework 없는 Mock/stub | 6/6 PASS | Qt offscreen; `tool-mock.log` |
| KT-ND3 구형 Explorer 단독 반복 | 10/10 PASS | 회차별 새 namespace, 1.086~1.343초; `explorer-results.json`, `explorer-01.log`부터 `explorer-10.log` |
| KT-ND2 Virtual LAN Diagnostics E2E | PASS | 두 격리 Host, 별도 Tool Observer 프로세스, Qt offscreen; `/tmp/kt-nd2-virtual-final/results.json` |
| KT-ND3 Virtual LAN 후속 회귀 | PASS | `/tmp/kt-nd3/virtual/results.json`; KT-ND2 측정값과 별도 실행 |
| KT-ND3 Framework Network | 최초 7/8 PASS, 실패 1개 단독 재실행 PASS | Topic Integration 대기 실패; `framework-topic_data.log`, `framework-topic_data-retry.log` |

상대 로그 이름은 `/tmp/kt-nd3/` 기준이다. `/tmp`는 임시 개발 artifact이며 배포에 포함되지 않는다.
Framework F-ND5A의 별도 8/8 PASS 기록은 Framework의
`docs/06_VERIFICATION_AND_LIMITATIONS.md`에 있으며 위 KT-ND3 최초 실패를 대체하지 않는다.

#### KT-ND5A 최종 버전 정합화 검증

`tests/network_backend.cpp`의 고정 `5.2.0` 기대값을 테스트에 사용한
`DiscoveryConfig.profile.framework`의 major/minor/patch 조합으로 바꿔 실제 관측 버전과 비교한다.
NetworkProtocolVersion **1.0** 및 metadata/identity/session/compatibility 검증은 유지했다.
제품 코드·Framework·synthetic Graph 버전 테스트는 변경하지 않았다.

| 구성 | 결과 | 소요 시간 | 출처 |
| --- | --- | --- | --- |
| Framework v5.3 working tree + Tool | 19/19 PASS | 51.51초 | `/tmp/kt-nd5a/real-tests.log` |
| Framework v5.2 Discovery-only + Tool | 18/18 PASS | 29.03초 | `/tmp/kt-nd5a/legacy-tests.log` |
| Mock/stub | 6/6 PASS | 2.14초 | `/tmp/kt-nd5a/mock-tests.log` |

서로 분리된 빌드 디렉터리에서 실행했으며 **세 구성 모두 최초 실행 PASS**, 실패·재실행은 없었다.
Framework 구성은 격리 Loopback/PID/SHM 환경, GUI는 Qt offscreen이다.
이 기록은 KT-ND2/ND3와 별도이며, KT-ND5A에서 Virtual LAN E2E·물리 LAN·수동 GUI를 재실행하지 않았다.
현재 v5.3은 여전히 미커밋 working tree이며 이 검증을 대표하는 공개 SHA를 지정하지 않는다.

#### KT-ND2 실제 Virtual LAN 측정

출처: `/tmp/kt-nd2-virtual-final/results.json`의 `real_backend_rates_graph`.
약 200 Hz Publisher와 Gateway max_hz=50 조건에서 관측한 값이며 정확한 50.0 Hz를 요구하지 않았다.

| 관측 | 실제 측정값 |
| --- | --- |
| Local Publisher | 200.22 Hz |
| Network Send | 46.67 Hz |
| Network Receive | 47.07 Hz |
| Proxy Publish | 47.07 Hz |
| Local Skipped | 152.58/s |

Publisher는 fixture 발행 횟수/실제 경과 시간, 나머지는 Gateway Counter Delta/Tool 관측 시간이다.
송신·수신 Gateway의 관측 구간은 독립적이며 동일 시각이나 동일 수치를 가정하지 않는다.
Skipped는 Local sampling 생략이며 Network packet loss가 아니다. 성능 보장 수치가 아니다.

양쪽 **ACTIVE 기록 2개가 동일 Route로 일치**, Graph의 방향성 Network Edge는 **1개**였다.
Counter 증가와 실제 Proxy Subscriber 수신, Edge 양 끝·방향·타입·Route ID/Hz tooltip을 확인했다.
Gateway LOST·재시작/session 교체, 명시적 Query Timeout, Diagnostics 비활성화 후 Refresh에서
잘못된 선이 남지 않았다. 11초 대기 중 기존 선 유지, 같은 Snapshot Refresh 후 10초 freshness 거부도 PASS했다.

### 실패 이력과 남은 제한

- **Historical — KT-ND4 당시 미완료 항목:** `tests/network_backend.cpp:37`에 `5.2.0`
  고정 기대값이 남아 있었고 Framework 기본값은 `5.3.0`이었다. 당시에는 버전 불일치를 예상해
  정합화·재검증을 후속 과제로 기록했으며, 기존 KT-ND3 19/19를 새 v5.3 검증으로 간주하지 않았다.
  **이 항목은 이후 KT-ND5A에서 해결됐다.** 현재 비교 방식과 새 실행 결과는 위 KT-ND5A 절을 따른다.

- KT-ND2 구형 Framework 회귀는 최초 **17/18**, Explorer 초기 대기에서 실패했다.
  단독 재실행 PASS, KT-ND3 10/10 및 전체 18/18 PASS로 후속 확인했지만 원인은 미확정이다.
  `/tmp/kt-nd3/prior-kt-nd2-legacy-failure.log`를 보존했다. 최초부터 18/18이었던 것으로 바꾸지 않는다.
- KT-ND3 Framework는 최초 **7/8**, Topic `Integration()`의 `Until` assertion 실패 후
  단독 재실행 PASS했다. 어떤 lambda인지 로그로 확정할 수 없으며 원인은 미확정이다.
- F-ND4B Topic `Conflict()` 대기 실패(`/tmp/kcf-fnd4b-regression.log`) 후
  단독 PASS(`/tmp/kcf-fnd4b-final-topic.log`), Action `Integration()` 대기 실패
  (`/tmp/kcf-fnd4b-regression-recheck.log`) 후 단독 PASS(`/tmp/kcf-fnd4b-action-recheck.log`) 이력을 보존한다.
  CPU 부하·Discovery·프로세스 순서·namespace 충돌을 원인으로 단정하지 않는다.
- 후속 PASS는 간헐적 실패의 해결을 증명하지 않는다. 관련 제품 코드와 Timeout 기본값은 변경하지 않았다.
- **물리 LAN과 실제 데스크톱 GUI 수동 검증은 미실행**이다. Virtual LAN/Qt offscreen 결과로 대체하지 않는다.
- Graph는 수동 Refresh이므로 표시 후 자동 만료되지 않는다. 양쪽 10초 이내 증거는 그래프 재생성 시 검사한다.
- truncated Snapshot은 부분 성공이며 전체 Route 목록이나 모든 이력을 보장하지 않는다.
- KT-ND3에서 발견했던 Framework의 오래된 단계별 주석은 이후 F-ND5A/B 문서 정리 대상이었다.
  이 Tool 문서 작업은 Framework를 수정하지 않는다.

### Historical 보존

[v0.1](V0_1_RELEASE_NOTES.md), [v0.11](V0_11_RELEASE_NOTES.md),
[v0.12](V0_12_RELEASE_NOTES.md) 릴리스 노트는 변경하지 않는다.
Framework v5.2 `d41fc2fc71d3b60b0475035ec56ac32b92ffb849`와 이전
`0a3f2a630cec209793f9ac6e99107752e9e01283`의 SHA·날짜·18/18 검증은 Historical이다.
v0.12 당시 외부 Diagnostics 미지원은 당시 사실이며 v0.13의 현재 계약과 구분한다.
