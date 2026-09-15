# Changelog

KCF Tool과 KSS Control Framework의 단계별 누적 작업 요약입니다.
개별 작업일을 소급 지정하지 않고 진행 순서대로 정리했습니다.

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

## v0.1 — 현재 상태 버전 지정

- KT-8 검증 완료 상태를 KCF Tool v0.1 기준으로 지정했습니다.
- KT-0~KT-8 및 KT-7.1 hardening을 포함하며, Framework R1~R5 + R4.1 + R2B.1과 연동합니다.
- Application/Element Explorer, Topic Echo, Parameter Viewer/Editor, Service Call 및 identity/stale 처리를 포함합니다.
- README에 버전을 명시하고 [v0.1 릴리스 노트](docs/V0_1_RELEASE_NOTES.md)에 검증 결과, 호환성 및 제한을 정리했습니다.
- 이번 버전 지정은 문서에 반영했으며 git tag 생성, commit/push 및 배포는 수행하지 않았습니다.

## v0.1 검증 상태

- Framework: R1~R5, R4.1, R2B.1 및 lifecycle·supervision·Supervisor loss·Reset/recovery·SystemStatus·IPC·Integration PASS.
- Tool: KT-8 generic 다중 Application 검증 1/1 PASS, 기존 회귀 10/10 PASS, KCF 없는 GUI 3/3 및 backend-only 1/1 PASS. KT-7 실제 다중 Application GUI는 R2B.1 단계에서 PASS.
- SHM format은 3, Service protocol은 2를 유지합니다.
- scope 적용에는 Supervisor와 child 바이너리를 함께 rebuild/restart해야 합니다.
- Graph/Launch와 별도 dynamic callback monitor는 미구현 상태입니다.
- 기존 변경은 보존했으며 commit/push는 수행하지 않았습니다.

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

KT-7의 기존 보고서는 수정하지 않은 과거 기록이며, 현재 최종 상태는 R2B.1 검증 결과를 따릅니다.
