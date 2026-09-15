# KT-6 Service Call UI Result

## 사용과 경계

기존 5개 탭 뒤에 Services 탭을 추가했습니다. Refresh는 ToolBackend::QueryServices 결과를
identity별로 표시합니다. metadata에는 service name, runtime PID/start ticks, registration ID,
port/service_id, request/response diagnostic type 및 stable type_id를 표시합니다.

선택 시 ToolBackend::GetTypeTemplate으로 request/response form을 준비합니다.
Request는 Field Name / Type / Input, Response는 Field Name / Type / Value입니다.
scalar와 fixed array를 지원하고 배열은 `values[0]` 형태로 펼칩니다.
response template은 이름/type만 표시하며 값은 Call 성공 전까지 빈칸입니다.
request 또는 response type_id가 0이면 `Stable service type descriptor unavailable`을 표시하고
Call을 비활성화합니다. Field 이름/type/배열 길이는 GUI에서 편집하지 않습니다.

Parameter와 Service form은 동일한 FieldValue/DataSnapshot row traversal 및 input extraction을 사용합니다.
ValidateService는 KT-5 ValidateParameter와 같은 KT-3 TypeDescriptor/Encode 로직을 재사용하며
UDP 요청을 보내지 않습니다. invalid input이면 GUI는 CallService를 호출하지 않습니다.
GUI에는 KCF IntrospectionClient/ServiceInfo/TypeDescriptor/DynamicServiceClient 헤더나 호출이 없습니다.
GUI의 ServiceInfo는 **Tool model**이며 Framework POD를 노출하지 않습니다.

입력, focus loss, 선택 변경은 네트워크 Call을 발생시키지 않습니다. 오직 Call 버튼만 실행합니다.
선택/Refresh 시 request를 초기화하고 이전 response를 비웁니다. 선택 identity가 사라지면
selection과 양쪽 form을 비웁니다. Request history/periodic call/traffic monitor는 없습니다.

## 비동기 실행과 수명

Service Call에 한정된 QThread::create worker를 사용합니다. 기존 ToolBackend는 직렬 호출을
전제로 하므로 Call 전에 Echo를 중지하고 Call 진행 중 탭 내부 조작을 일시적으로 비활성화합니다.
서비스 선택/request editor/Call/Refresh와 다른 backend operation이 worker와 경합하지 않습니다.
각 backend 접근 handler에도 busy guard가 있습니다. Echo는 Call 후 자동 재시작하지 않습니다.

worker는 ToolBackend::CallService와 process-local 결과만 처리합니다. Widget은 접근하지 않습니다.
QThread::finished의 **queued GUI callback**이 결과를 반영하고 조작을 재활성화합니다.
GUI event loop는 timeout/retry 중에도 계속 동작합니다. 새 generic task framework나 callback
subscription은 추가하지 않았습니다.

Call 진행 중 창 닫기는 완료 후 닫도록 안내하며 보류합니다. 복잡한 cancellation은 없습니다.
직접적인 window destruction 시에는 worker 종료를 확인한 후 backend를 해제하여 dangling access를 막습니다.
Call 버튼 비활성화와 busy guard로 한 GUI action의 동시 중복 요청을 막습니다.

## 결과와 timeout

각 Call은 기존 KcfBackend/DynamicServiceClient의 독립적인 request/response operation입니다.
GUI는 현재 200 ms timeout과 retry_count 2를 전달하고, **기존 KCF 내부 retry만** 사용합니다.
Tool에 추가 재호출 loop, timer, queued retry는 없습니다.

Call 시작/validation 실패 시 이전 response를 비웁니다. framework 오류는 response 성공으로
표시하지 않습니다. 성공 response 안의 `success=false`나 application code는 정상 payload로
표시하며 framework 실패로 해석하지 않습니다.

Timeout은 **Tool이 response를 받지 못했다는 뜻이며 server callback 미실행을 보장하지 않습니다.**
화면에는 `Server callback may have executed. No automatic retry.`를 표시합니다.
다시 실행하려면 사용자가 Call을 새로 눌러야 합니다.

Runtime 종료/registration/type/UDP validation 오류는 public backend 결과를 따릅니다.
오래된 metadata는 validation 또는 Call에서 실패할 수 있으며 response는 비웁니다.
다음 명시적인 Refresh에서 사라진 entry를 제거합니다.

## Mock과 테스트

Mock은 `/mock/add`와 `/mock/subtract`를 제공하며 int32 a/b → result로 같은 form/worker 경로를
사용합니다. 범위/shape/type를 검증하고 결과 overflow도 오류로 반환합니다.

실제 fixture의 request에는 int32, uint8, int8, float32, fixed float array를 사용하고
response에는 bool 및 array도 포함했습니다. 모든 11 primitive의 기본 encode/decode 검증은
기존 KT-3 codec 회귀에서 유지합니다.

새 real Qt Service smoke는 실제 서버와 ToolBackend 호출 counter를 사용합니다.
1.2초 지연 callback으로 timeout을 만들고 GUI heartbeat timer가 계속 실행되는지 확인합니다.
KCF 내부 retry가 있어도 callback이 중복 실행되지 않는지 response count로 확인합니다.
테스트용 Service와 fixture만 추가/확장했으며 Framework source/test는 변경하지 않았습니다.

| 항목 | 결과 |
|---|---|
| Build | 외부 KCF + Qt build PASS; backend 엄격 warning 구문 검사 PASS |
| Service Discovery | public Tool model 목록/metadata 표시 PASS |
| Service Selection | identity 선택, request/response template, undefined type 비활성 PASS |
| Request Form | scalar/array, Parameter와 공유하는 model/input 처리 PASS |
| Input Validation | uint8=300, int8=-200, float=abc에서 CallService 0회 PASS; malformed array/type ID 검증 PASS |
| Explicit Call | 2+3 → 5, 입력만으로 Call 없음 PASS |
| Async GUI | timeout 동안 GUI heartbeat ≥10회, widget 변경은 queued GUI callback PASS |
| Response Decode | known scalar/array 값과 success=false 정상 payload 표시 PASS |
| Framework Error | 실패 시 response clear 및 status 구분 PASS |
| Timeout | 지연 서버로 timeout, 안내 및 Call 재활성 PASS |
| No Automatic Retry | timeout 뒤 1초 대기에도 Tool Call count 불변 PASS |
| Double Call Protection | 진행 중 반복 클릭 10회에도 추가 Call 없음 PASS |
| Service Exit | 종료 후 validation 오류, 이전 response 제거, Refresh 후 목록 제거 PASS |
| Service Switch | 이전 request/response 값 초기화 PASS |
| MockBackend | 같은 async UI에서 add 결과 5 및 switch PASS |
| Real KCF | 정상/지연/undefined Service fixture PASS |
| Backend Boundary | GUI → ToolBackend only |
| Qt Smoke | 기존 KT-3/Echo/Parameter/Mock + 신규 Service 포함 7/7 PASS |
| KCF Core | NOT MODIFIED |
| mecanum | NOT MODIFIED |
| Service Monitoring | NOT IMPLEMENTED |
| Result | PASS |

최종 CTest: **7/7 PASS**, 5.25초. 로그: `build-kt3/Testing/Temporary/LastTest.log`.
KCF/Qt 없는 `build-kt3-mock` backend build 및 contract: **1/1 PASS**.
기존 offscreen 테스트의 탭 수 기대값만 5→6으로 갱신하고 기존 기능 검증은 유지했습니다.

```sh
cmake --build build-kt3 -j4
ctest --test-dir build-kt3 --output-on-failure
```

Problems: 현재 확인된 실패 없음. 단일 Call 실행 동안 backend 조작을 직렬화하고, 취소 기능은 없습니다.
수동 데스크톱 시각 검사는 offscreen 검증 범위 밖입니다.
Service side effect의 취소/undo, 위험도 분류, remote network control은 구현하지 않았습니다.

## Modified Files

- `CMakeLists.txt`, `README.md`
- `include/kcf_tool/backend/tool_backend.hpp`
- `include/kcf_tool/backend/kcf_backend.hpp`
- `include/kcf_tool/backend/mock_backend.hpp`
- `include/kcf_tool/ui/main_window.hpp`
- `src/backend/kcf_backend.cpp`
- `src/backend/kcf_backend_stub.cpp`
- `src/backend/mock_backend.cpp`
- `src/ui/main_window.cpp`
- `tests/smoke.cpp`
- `tests/real_smoke.cpp`
- `tests/real_fixture.cpp`
- `tests/service_smoke.cpp` (신규)
- `docs/KT6_SERVICE_CALL_UI.md` (신규)

기존 변경/untracked 상태를 보존했습니다. commit/push는 하지 않았습니다.
