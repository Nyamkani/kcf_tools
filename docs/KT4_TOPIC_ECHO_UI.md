# KT-4 Topic Echo UI Result

> 이 문서는 KT-4 당시 결과입니다. 동일 타입 SHM 재생성 제한은
> [KT-4.1](KT4_1_PERSISTENT_TOPIC_ECHO.md)에서 해결되어 KT-4 최종 판정은 PASS입니다.

## 사용 방법

KT-3와 같은 CMake 구성으로 빌드합니다. 실제 연결은 외부 R1–R5 소스를 지정합니다.

```sh
cmake -S . -B build-kt3 -DKCF_SOURCE_DIR=/tmp/kcf-r1-introspection \
  -DKCF_TOOL_BUILD_TESTS=ON -DCMAKE_PREFIX_PATH=/opt/Qt/6.9.3/gcc_64
cmake --build build-kt3 -j4
./build-kt3/kcf_tool --backend kcf
# 또는 --backend mock
ctest --test-dir build-kt3 --output-on-failure
```

Topics에서 PUBLISHER 항목을 선택하고 **Start Echo**를 누릅니다.
Field Name / Type / Value 표에 scalar와 `values[0]` 형태의 배열 원소를 표시합니다.
상태와 기존 metadata 영역에 payload sequence를 표시합니다.
100 ms는 표시 갱신 주기이며 Publisher의 제어/publish 주기를 설정하지 않습니다.
Stop은 마지막 표시를 유지하고 추가 읽기를 중지합니다.
Topic 변경과 Refresh는 timer/준비 상태를 해제하고 표를 비웁니다. 자동 재시작하지 않습니다.
창 닫기 및 소멸 시에도 polling을 중지합니다.

목록에 endpoint role/PID를 표시하고, 상세에 start ticks/registration ID/type ID를 표시합니다.
읽기와 Refresh 후 선택 유지는 이름 대신 EndpointIdentity를 사용합니다.
Subscriber 항목을 선택하면 같은 이름의 publisher로 자동 전환하지 않고 Echo를 비활성화합니다.
`type_id == 0`에는 `Type descriptor unavailable`을 표시합니다.
공개 descriptor/storage 검증에 실패한 publisher도 Start를 비활성화합니다.

## Backend 경계와 polling

GUI는 ToolBackend만 사용합니다. DynamicTopicReader, IntrospectionClient, TypeDescriptor를
GUI에서 include하거나 직접 호출하지 않습니다.

최소한의 동기식 `PrepareTopicRead(identity)` / `CloseTopicRead()` 계약을 추가했습니다.
선택 시 지원 여부를 검사하며 Start 시 descriptor와 process lifetime handle을 준비합니다.
준비한 identity의 ReadTopicLatest는 캐시한 descriptor로 decode합니다.
**Timer 경로에서는 ListRuntimes, ListEndpoints, GetType을 호출하지 않습니다.**
명시적인 Refresh는 기존 discovery를 실행하며 준비 상태를 폐기합니다.
준비하지 않은 기존 단발 ReadTopicLatest는 KT-3 동작을 유지합니다.

매 tick에 public DynamicTopicReader를 Open/Read/Close합니다. 오래된 mmap을 계속 읽는 대신
unlink, dead owner, 타입/layout 검증 실패를 공개 API에서 확인합니다.
KCF 구현의 Open은 nonblocking storage lock을 사용하고 Topic copy는 최대 32회 재시도로 제한됩니다.
현재 작은 Tool payload에는 GUI thread의 Qt timer를 사용합니다. 새로운 worker나 callback monitor는 없습니다.
큰 payload/많은 배열 원소의 UI 성능까지 보장하는 deadline은 추가하지 않았습니다.

선택 runtime의 Linux pidfd를 Prepare 때 확보한 뒤 poll(timeout=0)로 종료를 감지합니다.
PID 재사용 시 이전 runtime과 혼동하지 않습니다. 이 경로는 pidfd_open을 지원하는 Linux가 필요하며,
미지원/권한 오류가 나면 Echo 준비 실패로 표시합니다. KCF의 IPC 저장 구조나 physical name은 읽지 않습니다.

읽기 오류는 Echo를 중지하고 Echo 상태 및 status bar에 표시합니다. 자동 재접속하지 않습니다.
사용자가 Refresh/재선택/Start로 다시 시도할 수 있습니다.

Mock의 KT-3 구현은 payload에 `-ENOTSUP`을 반환하고 있었습니다.
이번 단계에서 기존 metadata/Parameter/Monitor 계약을 유지하면서 identity 기반 로컬 샘플을 추가했습니다.
sequence는 성공한 Mock read마다 증가하고 scalar 및 배열을 반환합니다.
Mock type IDs는 시연용이며 실제 Framework ABI 등록 ID가 아닙니다.

## 검증 결과 (2026-09-14)

| 항목 | 결과 |
|---|---|
| Build | 외부 KCF + Qt build PASS; backend -Wall/-Wextra/-Wpedantic/-Werror 구문 검사 PASS |
| Topic Selection | endpoint identity 사용, duplicate publisher/subscriber 구분 및 Refresh 후 identity 유지 PASS |
| Start / Stop Echo | 즉시 첫 읽기, Stop 이후 읽기 없음, Refresh/close 시 중지 PASS |
| Polling | Qt timer 100 ms; 준비된 경로의 discovery/GetType 호출 없음(코드 확인) |
| Field Display | scalar 및 fixed array, payload sequence 표시 PASS |
| MockBackend | 동일 UI 경로의 읽기/갱신/Stop/Topic switch PASS |
| Real KCF | Sample의 sequence/x/count/values 표시; SIGUSR1으로 값 변경 후 다음 polling 반영 PASS |
| Publisher Exit | runtime 종료 후 오류 상태 및 timer 중지 PASS |
| Topic Removal | runtime 생존 중 unlink 후 오류 상태 및 timer 중지 PASS |
| Topic Switch | 이전 timer/표 해제, 새 identity 값 표시 PASS |
| Undefined Type | Start 비활성 및 descriptor unavailable 메시지 PASS |
| Backend Boundary | GUI → ToolBackend만 사용 |
| Qt Smoke | 기존 동작 회귀 및 Mock/실제 Echo offscreen PASS |
| KCF Core | NOT MODIFIED; R5 기준 129개 source 파일 SHA256 일치 |
| mecanum | NOT MODIFIED; 기존 사용자 변경 유지 |
| Dynamic Monitor | NOT IMPLEMENTED / -ENOTSUP |

CTest는 5/5 PASS입니다: backend contract, Mock Qt smoke, KT-3 real backend integration,
value codec, real Qt smoke. 기존 Echo 미구현 assertion은 새 동작으로 갱신하고 다른 회귀 검증은 유지했습니다.
KCF/Qt 없는 backend 구성도 build 및 CTest 1/1 PASS입니다.
테스트는 하드웨어 없이 dummy Publisher/Parameter/Service/Supervisor를 사용합니다.
실제 GUI의 시각적 배치는 offscreen 검증 범위 밖입니다.

## Problems / 남은 조건

**동일 프로세스·동일 타입의 SHM 재생성은 완전하게 검출할 수 없습니다.**
삭제 상태가 polling에서 관측되면 오류로 중지합니다. 타입/layout이 달라진 새 storage는 KCF의
검증 오류로 중지합니다. 그러나 동일 runtime이 같은 이름/타입/layout으로 storage를 polling 사이에
삭제·재생성하면 현재 public Open은 이를 유효한 storage로 받아들입니다.
R4 public API에는 storage generation을 조회하거나 Open 대상 generation을 고정하는 계약이 없습니다.
따라서 이 경우에도 반드시 Echo를 중지하라는 요구는 현 API만으로 충족하지 못했습니다.
완전한 보장에는 Framework의 public storage generation 계약이 필요합니다.
KCF core 수정 금지와 physical storage 직접 접근 금지를 지켰습니다.

**Result: FAIL (전체 요구 기준)** — 구현 및 실행한 5개 테스트는 PASS이나,
위 SHM 재생성 감지 조건이 남아 있어 무조건적인 KT-4 PASS로 보고하지 않습니다.

## Modified Files

- `README.md`, `docs/KT4_TOPIC_ECHO_UI.md`
- `include/kcf_tool/backend/tool_backend.hpp`
- `include/kcf_tool/backend/kcf_backend.hpp`
- `include/kcf_tool/backend/mock_backend.hpp`
- `include/kcf_tool/ui/main_window.hpp`
- `src/backend/kcf_backend.cpp`
- `src/backend/kcf_backend_stub.cpp`
- `src/backend/mock_backend.cpp`
- `src/ui/main_window.cpp`
- `tests/backend_contract.cpp`
- `tests/smoke.cpp`
- `tests/real_smoke.cpp`
- `tests/real_fixture.cpp`

CMake 구성, KCF core, mecanum, Service GUI, Parameter editor는 변경하지 않았습니다.
기존 작업 파일과 untracked 상태를 보존했습니다. commit/push는 하지 않았습니다.
