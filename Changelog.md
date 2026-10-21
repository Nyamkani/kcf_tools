# Changelog

KCF Tool과 KSS Control Framework의 단계별 누적 작업 요약입니다.
개별 작업일을 소급 지정하지 않고 기존 단계별 이력은 진행 순서대로 보존합니다. 최신 버전 요약은 상단에 구분합니다.

## v0.14 — Generic Remote Control / Multi-Host UI

- Tool 버전을 v0.14 / CMake 0.14.0으로 정합화하고 Framework v5.4를 연동 기준으로 문서화했습니다.
- Multi-Host Discovery/Host 선택과 metadata 기반 Remote Application/Element/Endpoint 탐색을 제공합니다.
- Remote Parameter Get/Set/Readback, Service Call/Response를 제공합니다.
- Remote Action Goal/Feedback/Result/Cancel과 ACK/Terminal State 구분을 제공합니다.
- Remote Topic Subscribe/Unsubscribe, Payload/Sequence 및 수신률을 표시합니다.
- TypeDescriptor 기반 generic scalar/fixed-array 입력·출력과 기존 Diagnostics/Verified Route Graph를 유지합니다.
- 이 기능은 v0.13 후속 개발 이력을 이어받으며, 이번 정합화에서 제품 동작·Network 프로토콜·빌드 경로는 변경하지 않습니다.
- 2026-10-21 물리 LAN 결과는 당시 Framework v5.3 / Tools v0.13 기록이며 v0.14 재검증으로 소급하지 않습니다.
- 이번 실제 빌드·테스트 결과와 Framework 커밋 기준은 [v0.14 릴리스 노트](docs/V0_14_RELEASE_NOTES.md)에 기록합니다.
- 아래 v0.13 및 이전 항목의 “현재/이번”은 각 작업 당시 기록입니다. PASS/FAIL·재실행 이력을 보존합니다.

## KT-0 — Independent Tool Project Base

- Framework와 독립적으로 빌드하는 C++17 / Qt Widgets Tool 프로젝트를 구성했습니다.
- ToolBackend, MockBackend와 Application·Element·Topic·Parameter 기본 화면을 만들었습니다.

## KT-1 — External KCF Access Contract

- 기존 backend/GUI 구조를 유지하면서 외부 KCF 접근에 필요한 계약을 문서화했습니다.
- Tool이 내부 SHM 구조를 직접 다루지 않고 공개 API를 사용하도록 경계를 정했습니다.

## KT-2 — KCF External Interface Gap Analysis

- 외부 접근 계약과 기존 Framework 구현의 차이를 분석했습니다.
- Runtime/Endpoint discovery, 타입 설명, 동적 데이터 접근에 필요한 Framework 확장을 정리했습니다.

## R1 — Public Runtime Discovery

- 외부 프로세스가 실행 중인 Runtime의 공개 metadata를 조회할 수 있게 했습니다.
- PID + process_start_ticks로 Runtime 세대를 구분하고 종료된 프로세스를 걸러냅니다.

## R2A — Element Self-Description / Endpoint Registry

- Runtime별 Topic·Parameter endpoint 등록과 조회를 추가했습니다.
- endpoint 이름, 역할, 타입 정보와 registration identity를 공개 metadata로 제공합니다.

## R2B — Supervisor Application Discovery / Membership

- Supervisor Application과 managed Element membership 조회를 추가했습니다.
- Supervisor membership과 Runtime을 PID + start_ticks로 연결하도록 했습니다.

## R3 — Stable Flat Type Descriptor

- stable type_id와 scalar·fixed array의 필드 이름, 타입, offset 등을 설명하는 descriptor를 추가했습니다.
- application struct를 compile-time에 알지 못해도 payload 구조를 조회할 수 있게 했습니다.

## R4 — Dynamic Topic Read / Dynamic Parameter Get-Set

- DynamicTopicReader와 DynamicParameterClient로 raw payload 읽기 및 Parameter 쓰기를 추가했습니다.
- storage type_id·크기·layout을 검증하고 기존 typed IPC 및 synchronization 경로를 재사용합니다.

## R5 — Service Introspection / Dynamic Service Call

- Service endpoint 및 request/response descriptor 조회와 동적 호출을 추가했습니다.
- 타입 검증, timeout/retry 및 duplicate 처리 계약을 검증했습니다.

## KT-3 — Real KCF Backend / Tool Interface Integration

- 실제 KcfBackend를 연결하고 discovery·descriptor·동적 접근 공개 API를 Tool 모델로 변환했습니다.
- KCF 없는 환경의 Mock/stub 빌드와 기존 GUI 독립성을 유지했습니다.

## KT-4 — Topic Echo UI

- Topic 선택 후 latest payload를 주기적으로 조회해 scalar·array 값을 표시하는 Echo 화면을 추가했습니다.
- Start/Stop과 타입 미지원·읽기 오류 표시를 연결했습니다.

## R4.1 — Dynamic SHM Recreate / Incarnation Detection

- 같은 이름과 타입으로 SHM이 재생성된 경우에도 기존 storage와 새 incarnation을 구분하도록 했습니다.
- 오래된 dynamic handle의 접근을 감지하고 명확한 오류를 반환하도록 보완했습니다.

## KT-4.1 — Persistent Topic Echo Session

- Echo 동안 하나의 reader/session을 유지하고 주기적으로 ReadLatest를 호출하도록 변경했습니다.
- R4.1 재생성 감지 시 Echo를 중지하고 Refresh 후 다시 시작하도록 했습니다.

## KT-5 — Parameter Viewer / Editor

- descriptor 기반 scalar·fixed array 조회와 필드별 편집을 추가했습니다.
- 입력 검증, Apply/Revert, 쓰기 후 재조회 및 미저장 편집 보호를 구현했습니다.

## KT-6 — Service Call UI

- Service 선택, request 입력, 명시적 Call 및 response 표시 화면을 추가했습니다.
- worker 호출로 GUI 응답성을 유지하고 중복 조작·잘못된 입력·timeout을 처리했습니다.

## KT-7 — Generic Application / Element Explorer

- Application별 managed Runtime hierarchy와 별도 Standalone 그룹을 추가했습니다.
- Element 상세의 Topic·Parameter·Service 목록을 기존 Echo·Editor·Call 화면으로 연결했습니다.
- Refresh와 Reset 시 identity를 기준으로 선택을 보존하거나 안전하게 해제합니다.
- 최초 검증에서는 전역 SystemStatus의 `-EEXIST` 때문에 실제 Application 두 개 동시 실행 항목이 FAIL이었습니다.

## R2B.1 — Application-Scoped SystemStatus

- SystemStatus를 Supervisor PID + start_ticks별 scope로 분리하고 child exec 환경으로 전달했습니다.
- Reset에서도 같은 scope/storage를 유지하며, 다른 Application의 상태와 정리에 영향을 주지 않습니다.
- 같은 이름의 Supervisor 동시 실행, 양방향 ERROR 격리, Reset, 종료 및 crash/stale 재시작을 검증했습니다.
- 기존 child의 `SystemStatusSubscriber::Open()`을 유지했습니다. 외부 observer는 `OpenForApplication(pid, start_ticks)`로 대상을 지정합니다.
- **Tool 코드 수정 없이 실제 Application A·B와 Standalone 동시 실행을 확인하여 KT-7 최종 PASS로 전환했습니다.**

## KT-7.1 — Pre-v0.1 Hardening

- 실제 Topic/Parameter/Service 접근 전후에 cached registration identity와 현재 registry를 재검증합니다.
- stale registration은 `-ESTALE`로 거부하며, 정상 Echo polling과 기존 R4.1 storage 보호는 유지합니다.
- Parameter/Service float 입력을 finite 값으로 제한하고, 미측정 heartbeat/frequency 및 sample 이전 sequence를 `N/A`로 표시합니다.
- README의 KT-7 상태를 최종 PASS로 동기화하고 Framework revision·rebuild 및 외부 SystemStatus observer 계약을 명시했습니다.

## KT-8 — Generic Cross-Application Validation

- Tool/Core 바깥의 `/tmp/kcf_generic_validation`에 서로 다른 타입을 사용하는 Alpha·Beta Application과 Standalone Runtime을 작성했습니다.
- 기존 Tool 코드 수정 없이 동시 discovery, PID + start_ticks membership, 같은 이름의 Supervisor instance 구분을 검증했습니다.
- descriptor 기반 Topic Echo, Parameter scalar·array 편집과 typed readback/watcher, 서로 다른 Service 입력·응답을 검증했습니다.
- Application 간 상태·IPC 격리, Reset 세대 교체, Endpoint 재등록, 종료·재시작 및 stale 접근 처리를 확인했습니다.
- KT-8 통합 테스트 1/1, 기존 Tool 10/10, KCF-less GUI 3/3·backend 1/1, Framework 회귀 모두 PASS입니다.
- 기존 회귀는 KT-8 프로세스 종료 후 순차 실행했습니다. Tool production 및 KCF/Bringup 소스는 시작 시점의 해시와 동일하며 mecanum도 수정하지 않았습니다.
- 결과 보고서를 추가하고 README에 독립 프로젝트 빌드, 자동 검증 및 Application과 Tool의 수동 실행 방법을 정리했습니다.

## v0.1 — 기존 개발 기준

- KT-8 검증 완료 상태를 KCF Tool v0.1 기준으로 지정했습니다.
- KT-0–KT-8 및 KT-7.1 hardening을 포함하며, Framework R1–R5 + R4.1 + R2B.1과 연동합니다.
- Application/Element Explorer, Topic Echo, Parameter Viewer/Editor, Service Call 및 identity/stale 처리를 포함합니다.
- README에 버전을 명시하고 [v0.1 릴리스 노트](docs/V0_1_RELEASE_NOTES.md)에 검증 결과, 호환성 및 제한을 정리했습니다.
- 이번 버전 지정은 문서에 반영했으며 git tag 생성, commit/push 및 배포는 수행하지 않았습니다.

## v0.1 당시 검증 상태

- Framework: R1–R5, R4.1, R2B.1 및 lifecycle·supervision·Supervisor loss·Reset/recovery·SystemStatus·IPC·Integration PASS.
- Tool: KT-8 generic 다중 Application 검증 1/1 PASS, 기존 회귀 10/10 PASS, KCF 없는 GUI 3/3 및 backend-only 1/1 PASS. KT-7 실제 다중 Application GUI는 R2B.1 단계에서 PASS.
- SHM format은 3, Service protocol은 2를 유지합니다.
- scope 적용에는 Supervisor와 child 바이너리를 함께 rebuild/restart해야 합니다.
- Graph/Launch와 별도 dynamic callback monitor는 미구현 상태입니다.
- 기존 변경은 보존했으며 commit/push는 수행하지 않았습니다.

## v0.11 — Framework v5.1 연동 검증 완료

- 당시 Tool 버전을 v0.11로 지정하고 v0.1 기록은 기존 개발 이력으로 보존했습니다.
- Framework 기준은 `Nyamkani/kss_control_framework` dev의 `8360b6e053472ba20acdfc62b7489e33122997ec`입니다.
- Topic Format 4의 Depth=1/4 latest Echo, payload·Sequence 및 재생성 후 STALE/재연결을 검증했습니다. 기존 `ReadLatest()` 방식을 유지합니다.
- 자동 검증: 지연·반복 재연결 회귀와 Qt offscreen smoke를 포함하여 **14/14 PASS**입니다.
- 수동 검증(사용자 확인): 실제 GUI의 값 갱신과 SHM 재생성 후 Refresh·재선택·Echo 재연결을 확인했습니다. 자동 테스트 결과와 구분합니다.
- 구버전 GUI 혼용에 따른 Protocol error는 TypeRegistry SHM 크기 검사 실패였습니다. [진단 문서](docs/V51_MANUAL_RECONNECT_DIAGNOSIS.md)에 근거를 기록했습니다.
- [v0.11 릴리스 노트](docs/V0_11_RELEASE_NOTES.md)를 추가했습니다. 이번 정리에서는 기능 코드 수정·재테스트·commit/push/tag 생성 및 GitHub Release 생성을 수행하지 않았습니다.

## v0.12 — Graph / Remote Discovery / Read-only Detail (2026-09-29)

- 현재 Tool 버전을 v0.12로 지정하고 v0.1·v0.11 개발 이력을 보존했습니다.
- Qt QGraphicsView/Scene 기반 Graph 탭을 추가했습니다. Runtime identity로 Application·Standalone과 Element·Topic을 연결하며 동일 이름의 여러 registration을 보존합니다.
- 발견된 Parameter OWNER/CLIENT와 Service SERVER 관계, 표시 toggle·Application 필터, deterministic layout, Zoom/Pan/Fit 및 기존 Element/Topic 화면 이동을 제공합니다.
- 공개 PeerDiscovery API를 사용하는 비동기 Network observer를 추가했습니다. Host + boot + PID/start_ticks + registration으로 Remote metadata를 구분하며 수동 Refresh로 표시합니다.
- Remote Host/Application/Element와 Topic·Parameter·Service·Action metadata를 표시합니다. 읽기 전용 상세에서 ONLINE/LOST, 버전, compatibility, capabilities 및 type 정보를 확인합니다.
- Host·Local/Remote·T/P/S/A 필터를 제공하고, Remote 노드는 기존 Local 쓰기·호출 경로로 연결하지 않습니다. 자체 wire protocol이나 forwarding은 추가하지 않았습니다.
- 검토에서 Remote 선택 해제·Local 선택 후 이전 상세가 남는 문제를 재현하여 수정하고 회귀 테스트를 추가했습니다.
- 자동 검증: KCF + Qt **18/18 PASS**, Framework 없는 Mock/stub + Qt **6/6 PASS**. 실제 Backend는 loopback, GUI는 offscreen 검증입니다.
- Framework HEAD는 `8360b6e053472ba20acdfc62b7489e33122997ec`이지만 Network/Action 및 Core의 기존 미커밋 변경이 포함된 working tree 기준입니다. clean v5.1 commit 검증과 구분하며, 이번 Tool 검토에서 Framework 파일은 변경하지 않았습니다.
- 실제 물리 LAN·데스크톱 수동 검증은 미수행입니다. 외부 Gateway route/statistics·proxy 판별, Remote Set/Call/Goal/Cancel 및 Launch는 미지원입니다.
- [Graph 기록](docs/GRAPH.md), [Network 관측 및 검토 기록](docs/NETWORK_OBSERVATION.md), [v0.12 릴리스 노트](docs/V0_12_RELEASE_NOTES.md)를 연결했습니다.
- 이번 버전 정리는 문서만 변경했습니다. 재테스트·commit/push/tag 생성 및 GitHub Release 생성은 수행하지 않았습니다.

## v0.12 — 공개 KCF v5.2 0a3f2a6 기준 정합화 (과거 후속 작업)

- Tool v0.12 main `f4fb4471fc0b03b24663eb20eede8d8491fd87f6`와 Framework v5.2 dev `0a3f2a630cec209793f9ac6e99107752e9e01283`의 공개 상태를 확인했습니다.
- README·v0.12 릴리스 노트·Network 문서의 현재 빌드 기준을 공개 KCF v5.2로 정리했습니다. 위 v0.12 최초 검증 및 commit/push 미수행 문구는 당시 작업 기록입니다.
- Local v0.11의 14/14, Local Graph 당시 SHA, 미커밋 Network working tree의 18/18·6/6 및 Framework 215개 해시 기록을 보존했습니다. 후속 공개 커밋 검증은 별도로 기록합니다.
- 후속 검증: 공개 `0a3f2a6`의 수정 없는 Git 추출본에서 새 configure/build 및 KCF + Qt **18/18 PASS**, Framework 없는 Mock/stub + Qt **6/6 PASS**. 원래 checkout의 동시 외부 변경이 포함된 중간 실행은 제외했으며 [Network 기록](docs/NETWORK_OBSERVATION.md)에 구분했습니다.
- CMake의 KCF_SOURCE_DIR 도움말만 변경했습니다. header 존재 기반 Network 감지, 기능 코드 및 Framework 소스는 수정하지 않습니다. Tool 버전은 v0.12를 유지합니다.
- Git tag/GitHub Release 생성 여부는 확인하지 않았으며, 이번 작업에서는 commit/push를 수행하지 않습니다.

## v0.12 — KCF v5.2 d41fc2f 최종 정합화

- 현재 기준은 **kcf_tools main / v0.12**와 KCF `d41fc2fc71d3b60b0475035ec56ac32b92ffb849`입니다. Tool 자신의 SHA는 최초 기능 반영 등 과거 이력으로만 유지합니다.
- 실제 Framework 연동 테스트 `network_backend`의 FrameworkVersion 기대값을 5.1.0에서 5.2.0으로 수정했습니다. NetworkProtocolVersion 1.0과 `network_graph`의 synthetic 5.1.0 데이터는 유지합니다.
- 현재형 문서 baseline을 d41fc2f로 정리하고, 과거 0a3f2a6 clean 기준 18/18 및 중간 17/18(실제 5.2.0 / 기대 5.1.0)의 원인 기록을 보존했습니다.
- d41fc2f 독립 추출본으로 새 configure/build 및 KCF + Qt **18/18 PASS** (24.21초), Framework 없는 Mock/stub + Qt **6/6 PASS** (1.27초)를 확인했습니다. 로그와 조건은 [최종 검증 기록](docs/NETWORK_OBSERVATION.md)을 따릅니다.
- Framework 원본의 기존 변경은 보존했으며 추출본은 지정 커밋과 동일합니다. `git diff --check`를 통과했습니다.
- 기능 추가, Framework 수정 및 commit/push는 수행하지 않았습니다. 기존 지원 범위와 미검증 항목은 유지합니다.

## Local Topic Echo — Observed Publish Hz (2026-10-18)

- 기존 100 ms Echo polling에서 transport sequence 증가량 / `steady_clock` 경과 시간으로 Publish Hz를 표시합니다. 약 1초·최대 16개 관측점을 유지하고 최소 0.5초 전에는 `Measuring...`를 표시합니다.
- Stop·오류·Topic 변경·Refresh는 기록을 초기화하고 정적 Frequency 또는 N/A를 복원합니다. sequence 감소 시에도 재측정합니다. Backend API, fixture 및 Remote 코드는 변경하지 않았습니다.
- 새 configure/build(`/tmp/kcf-tools-local-hz`), Framework `3204b94c6d50bdee1afa13e7714db76cdc5c62a2` 기준 전체 **18/18 PASS**. 100 Hz real fixture의 Depth=1/4 관측값 **98.4~100.8 Hz**, 허용 범위 60~140 Hz. 로그: `Testing/Temporary/LastTest.log`.
- 최초 샌드박스 실행은 fixture UDP socket 생성 거부로 중단했습니다. socket 사용이 가능한 환경에서 PID/SHM 격리를 유지하여 위 전체 회귀를 재실행했습니다. GUI 검증은 offscreen입니다.
- UI polling 횟수, payload timestamp 기반 source rate, Remote network receive/transmit Hz와 구분합니다. Tool 버전은 유지하며 Framework 수정 및 commit/push는 수행하지 않았습니다.

## v0.13 — Network Diagnostics / Verified Route Graph

현재 Tool 버전은 v0.13, CMake project version은 0.13.0입니다. Framework 기준은
v5.3.0 dev의 F-ND5B 완료 미커밋 working tree이며 공개 SHA/tag로 표현하지 않습니다.
아래는 KT-ND1A부터 KT-ND3까지의 기존 실행 이력입니다. KT-ND4 문서 작업에서 테스트를 재실행한 결과가 아닙니다.

### KT-ND1A — Remote Network Diagnostics Backend

- 기존 PeerDiscovery와 worker에서 공개 NetworkDiagnosticsClient를 재사용합니다.
- Gateway별 Route 전체 identity/session과 9개 Counter, 관측 시각, total_routes/truncated를 보존합니다.
- Query timeout 80 ms, 자동 retry 없음, Gateway별 약 1초 목표의 공정한 순환 조회를 적용합니다.
- LOST·세대 변경·실패 시 오래된 통계를 무효화하며 구형 Discovery-only와 Mock/stub를 유지합니다.

### KT-ND1B — Network Topic Rate Calculation

- 성공 Counter Delta / 실제 steady_clock 경과 시간으로 Send/Receive/Proxy Publish Hz와 Skipped/s를 계산합니다.
- 최소 0.5초와 Measuring/Unavailable/정상 0 Hz를 구분합니다. 전체 identity·session·route_id·sending 역할이 일치해야 합니다.
- 오류·Counter 감소·Route 종료 또는 truncated 누락 구간에서 연속성을 끊으며 9개 원본 Counter를 보존합니다.

### KT-ND1C — Remote Topic Diagnostics Detail UI

- 기존 읽기 전용 Remote Detail에 Route별 상태, identity/session, error/reason, Counter와 Rate를 표시합니다.
- 여러 Route를 합산하지 않고, Proxy Publish를 Application 처리율로 표현하지 않습니다.

### KT-ND1D — Verified Network Route Graph

- 양쪽 정상 Gateway가 확인한 동일 ACTIVE Route와 실제 Publisher/Subscriber registration만 연결합니다.
- 10초 freshness, 전체 identity/session/route_id 및 역할을 검사합니다. Network는 청록색 방향 실선,
  metadata ownership은 점선이며 tooltip에 Route ID와 Send/Receive Hz를 표시합니다.
- 기존 필터/레이아웃/Zoom/Pan/Fit을 유지하고 수동 Refresh 이후 자동 만료는 추가하지 않았습니다.

### KT-ND2 — End-to-End Integration Validation

- 두 실제 Gateway와 독립 Tool Observer, 기존 Virtual LAN fixture, Qt offscreen Graph까지 검증했습니다.
- Publisher 200.22 Hz, Send 46.67 Hz, Receive/Proxy Publish 47.07 Hz, Skipped 152.58/s를 관측했습니다.
- 양쪽 ACTIVE 기록이 동일 Route로 일치하고 Graph Edge 1개를 확인했습니다. Timeout/LOST/새 session/
  Diagnostics 비활성화 및 11초 대기 후 Refresh의 freshness 처리는 PASS입니다.
- 구형 Framework 최초 17/18에서 Explorer 초기 대기 실패, 단독 재실행 PASS 이력을 보존합니다.

### KT-ND3 — Pre-release Stability Review

- 구형 Explorer 격리 반복 10/10 PASS(1.086~1.343초), 현재 Framework Tool 19/19,
  구형 Framework 18/18, Mock/stub 6/6 및 Virtual LAN 후속 회귀 PASS입니다.
- Framework Network 최초 7/8, Topic Integration 대기 실패 후 단독 재실행 PASS이며 원인은 미확정입니다.
  과거 Conflict/Action의 간헐적 실패·후속 PASS도 최초 성공으로 바꾸지 않습니다.
- 변경 범위를 확인하고 외부 Framework에 생성된 Python 캐시를 제거했습니다.
  테스트 스크립트의 캐시 생성 방지만 추가했으며 제품 기능/Timeout은 변경하지 않았습니다.
- 물리 LAN·실제 데스크톱 GUI 수동 검증은 미실행입니다.

검증 출처·미확정 실패·호환성은 [v0.13 릴리스 노트](docs/V0_13_RELEASE_NOTES.md)를 따릅니다.
기존 v0.1/v0.11/v0.12 릴리스 노트, 과거 날짜·SHA·수치는 보존합니다.
이번 버전·문서 정리에서 commit/push/tag/GitHub Release는 생성하지 않습니다.

## 상세 기록

- [KT-3 실제 backend](docs/KT3_REAL_KCF_BACKEND.md)
- [KT-4 Topic Echo](docs/KT4_TOPIC_ECHO_UI.md)
- [KT-4.1 Persistent Echo](docs/KT4_1_PERSISTENT_TOPIC_ECHO.md)
- [KT-5 Parameter Viewer / Editor](docs/KT5_PARAMETER_VIEWER_EDITOR.md)
- [KT-6 Service Call UI](docs/KT6_SERVICE_CALL_UI.md)
- [KT-7 최초 결과 및 당시 제한](docs/KT7_APPLICATION_ELEMENT_EXPLORER.md)
- R2B.1 및 KT-7 최종 PASS 보고서: Framework checkout의 `examples/introspection/R2B1.md` (별도 로컬 프로젝트 문서).
- [KT-7.1 Pre-v0.1 Hardening](docs/KT7_1_PRE_V01_HARDENING.md)
- [KT-8 Generic Cross-Application Validation](docs/KT8_GENERIC_CROSS_APPLICATION_VALIDATION.md)
- [v0.1 기존 릴리스 노트](docs/V0_1_RELEASE_NOTES.md)
- [v0.11 릴리스 노트](docs/V0_11_RELEASE_NOTES.md)
- [Framework v5.1 수동 재연결 진단](docs/V51_MANUAL_RECONNECT_DIAGNOSIS.md)

KT-7의 기존 보고서는 당시 기록이며, KT-7의 최종 PASS는 R2B.1 검증 결과를 따릅니다.
현재 Tool 버전과 Framework 연동 기준은 위 v0.14 기록을 따릅니다. v0.13 및 이전 기록은 Historical로 보존합니다.
