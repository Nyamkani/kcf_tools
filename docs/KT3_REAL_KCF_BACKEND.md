# KT-3 Real KCF Backend Result

## 구조와 외부 dependency

`GUI → ToolBackend → KcfBackend → KCF public APIs` 구조입니다.
GUI와 공개 Tool model에는 KCF POD/헤더가 없습니다. 실제 backend는
`IntrospectionClient`, `DynamicTopicReader`, `DynamicParameterClient`,
`DynamicServiceClient`, 공개 TypeDescriptor만 사용합니다.
SHM 이름, mmap, storage layout, pin, mutex, UDP wire format은 직접 다루지 않습니다.

`KCF_SOURCE_DIR`로 외부 Framework의 `kcf` CMake 디렉터리만 연결합니다.
통합 테스트 fixture만 외부 Bringup 소스를 참조합니다. 소스 복사는 없습니다.
Framework가 없으면 Mock 및 disconnected KCF stub을 빌드합니다.

## Discovery와 identity

`Refresh()`는 ListSupervisors/ListRuntimes 이후 각 runtime의 ListEndpoints/ListServices를
조회합니다. 마지막으로 public discovery를 다시 확인하여 종료된 process identity를
제외합니다. 개별 runtime 조회 실패는 해당 목록을 생략하고 진행합니다.
최상위 discovery 오류는 negative errno와 ERROR 상태를 반환하며 목록을 비웁니다.
Discovery는 원자적 snapshot이 아니므로 마지막 확인 직후에도 프로세스가 종료될 수 있습니다.

유효 supervisor/runtime이 하나 이상이면 CONNECTED, 없으면 DISCONNECTED입니다.
중앙 서버 접속이나 background discovery thread는 없습니다. 호출자는 backend 작업을 직렬화합니다.

Application에는 이름/state/supervisor PID와 start ticks/revision/managed count가 있습니다.
Element membership은 PID + process_start_ticks가 모두 일치할 때만 연결합니다.
Supervisor의 friendly name을 우선하며 standalone은 executable identity와 `Standalone` 그룹을
사용합니다. 아직 membership이 없는 supervised runtime은 `Unassigned`입니다.

EndpointIdentity는 runtime identity와 registration ID의 조합입니다. 동일 Topic/Parameter 이름도
runtime별 publisher/subscriber 또는 owner/client 항목을 보존합니다.
이름 기반 접근은 publisher/owner를 우선합니다. 우선순위가 같은 후보가 여러 개면
`-ENOTUNIQ`이며 identity 기반 API를 사용해야 합니다.

현재 공개 metadata에 없는 heartbeat, 주기, discovery sequence, timestamp는 0(미제공)입니다.
조회 목록의 Parameter metadata를 실제 값으로 간주하지 않습니다.

## Type와 bytes

`GetTypeTemplate(runtime, type_id, snapshot)`은 공개 GetType으로부터 영값 field template을
만듭니다. 실제 값은 Topic ReadLatest 또는 Parameter Get으로 읽어야 합니다.
Tool 소비자는 application struct나 KCF descriptor를 직접 포함할 필요가 없습니다.

`DataSnapshot`은 stable type_id/type_name, sequence, fields를 포함합니다.
각 FieldValue는 name, 정확한 type_name, ValueKind, scalar value 또는 array_values를 가집니다.
지원 type_name은 bool/int8/uint8/int16/uint16/int32/uint32/int64/uint64/float32/float64입니다.
배열은 descriptor array_count > 1이면 array_values에 문자열 원소를 저장하고 value는 비웁니다.
R3는 scalar와 길이 1 배열을 구분하지 않으므로 count 1은 scalar로 표현합니다.
정수는 64-bit 범위를 보존하며 부동소수는 classic locale 및 max_digits10으로 변환합니다.

Decode/encode는 공개 ValidateTypeDescriptor와 type ID, payload size, 모든 field bounds를
검사하고 memcpy로 복사합니다. 정렬되지 않은 raw pointer를 역참조하지 않습니다.
BOOL은 0/1 byte만 허용합니다. type_id 0은 `-ENOTSUP`입니다.

Set/Service request에는 정확한 전체 field 집합, 이름, ValueKind/type_name, 배열 길이와
scalar 범위가 필요합니다. unknown/duplicate/missing field와 잘못된 타입은 실패합니다.
실패한 출력 인자 연산은 기존 출력 값을 보존합니다.

## 실제 접근

```cpp
kcf_tool::KcfBackend backend;
if (backend.Refresh() == 0) {
    for (const auto& topic : backend.GetTopics()) {
        kcf_tool::DataSnapshot latest;
        const int result = backend.ReadTopicLatest(topic.identity, latest);
        // result == 0: latest.fields 사용; negative errno: 접근 실패
    }
}
```

Topic은 GetType → DynamicTopicReader::Open → ReadLatest → decode 경로입니다.
Parameter는 GetType → DynamicParameterClient::Open → Get/Set 경로입니다.
각 접근 시 public GetType으로 runtime generation을 확인하고 actual storage/wire validation을
Framework에 맡깁니다. cached endpoint라도 종료된 runtime이나 변경된 storage type에는 실패합니다.

Parameter Set은 입력 전체를 검증한 후 Get한 bytes를 seed로 field를 encode하여
padding/비필드 bytes를 보존합니다. 그 다음 기존 DynamicParameterClient::Set을 사용하므로
typed client 값과 watcher/version notification도 동일하게 갱신됩니다.
Get+Set 묶음은 원자적 compare-and-swap이 아니며 concurrent writer와는 last writer semantics입니다.
unsupported model은 쓰기 불가이고 readonly model은 `-EPERM`입니다.
KCF discovery에는 별도 사용자 권한/readonly 정책이 없으므로 유효 descriptor가 있는 Parameter를
writable로 표시합니다. Tool 전용 storage/write 경로는 없습니다.

기존 문자열 Set은 단일 scalar Parameter만 지원합니다. 복합 값은 identity와 전체 DataSnapshot으로
Set합니다. GUI는 복합 값 표시를 지원하고 기존 문자열 편집기는 비활성화합니다.

Service는 QueryServices → GetTypeTemplate(request) → request fields 수정 → CallService입니다.
CallService는 request encode → DynamicServiceClient → response decode를 수행합니다.
포트/service ID, request/response type IDs 및 진단 이름은 Tool ServiceInfo에 보존됩니다.
잘못된 입력은 callback 전송 전에 거부합니다.

**Service timeout은 server callback 미실행을 보장하지 않습니다.** Tool은 추가 재호출 정책을
넣지 않습니다. timeout_ms/retry_count는 KCF 내부 retry에만 전달합니다(기본 200 ms/2).

Topic Monitor/Start/Stop은 `-ENOTSUP`입니다. 동적 callback worker와 GUI Echo는 미구현입니다.
Topic observer crash/pin 및 Parameter recovery는 기존 Framework 계약을 상속합니다.

## 검증 결과 (2026-09-14)

| 항목 | 결과 |
|---|---|
| Build | 외부 KCF + Qt 구성 build PASS |
| KCF Dependency | 외부 source 참조, 복사 없음 |
| Connection | no runtime DISCONNECTED, discovery 이후 CONNECTED PASS |
| Applications | application/state/PID/revision/count 매핑 PASS |
| Elements | PID + start ticks membership/friendly name PASS |
| Topics | 중복 이름의 3개 endpoint 보존, publisher 우선 PASS |
| Parameters | owner/client 보존, owner 우선 PASS |
| Type Decode | 11 primitive, 64-bit extrema, fixed array, unaligned field PASS |
| Topic ReadLatest | known scalar/array payload 일치, undefined type 거부 PASS |
| Topic Monitor | NOT IMPLEMENTED / -ENOTSUP |
| Parameter Get | typed 초기값 읽기 PASS |
| Parameter Set | typed client 값/Service 경유 owner 값 및 watcher 통지 PASS |
| Services | public discovery와 request/response type template PASS |
| Dynamic Service Call | known response, malformed request handler 이전 거부 PASS |
| Standalone | standalone runtime 및 그룹 발견 PASS |
| Supervised | Supervisor + child와 standalone 동시 구분 PASS |
| Stale Process Handling | 종료 후 cached read/call 실패, 다음 Refresh 제거 PASS |
| MockBackend Regression | 원본 contract 테스트 PASS |
| Qt Smoke | 기존 Mock 및 실제 backend offscreen 테스트 PASS |
| KCF Core | NOT MODIFIED; R5 기준 129개 파일 SHA256 일치 |
| mecanum | NOT MODIFIED; 작업 전 기존 변경 유지 |
| GUI | 기존 5개 탭, 실제 Parameter Get/편집 가능 여부 및 backend 도움말만 최소 연결 |
| Compatibility | SHM format 3 / Service protocol 2 |
| Result | PASS |

실제 연결 구성 CTest: 5/5 PASS (`build-kt3/Testing/Temporary/LastTest.log`).
KCF 및 Qt 없는 구성(`KCF_SOURCE_DIR` 비움, `KCF_TOOL_BUILD_GUI=OFF`): build 및 CTest 1/1 PASS.
검증은 테스트용 dummy Element/Supervisor와 loopback Service를 사용하며 하드웨어가 필요하지 않습니다.
기존 contract/Mock smoke 소스는 그대로 두고 stub에 연결합니다. 실제 backend는 별도 integration 및
실제 Qt smoke로 검증합니다. 최종 종료 테스트는 Supervisor RUNNING 이후 종료를 요청합니다.

Problems: 현재 확인된 실패 없음. GUI visual inspection은 offscreen 테스트 범위 밖입니다.
Service GUI, Topic Monitor, 복합 Parameter editor, graph/launch는 이번 범위에서 미구현입니다.
실제 연결되는 모든 프로세스는 동일 R1–R5 Framework revision으로 rebuild + restart해야 합니다.

## 변경/추가 파일

- `.gitignore`, `CMakeLists.txt`, `README.md`
- `docs/KT3_REAL_KCF_BACKEND.md`
- `include/kcf_tool/backend/tool_backend.hpp`, `include/kcf_tool/backend/kcf_backend.hpp`
- `include/kcf_tool/model/application_info.hpp`, `element_info.hpp`, `topic_info.hpp`, `parameter_info.hpp`, `dynamic_value.hpp` (같은 model 디렉터리)
- `include/kcf_tool/model/endpoint_identity.hpp`, `service_info.hpp` (신규)
- `src/backend/kcf_backend.cpp`, `kcf_backend_stub.cpp`, `value_codec.hpp`, `value_codec.cpp` (같은 backend 디렉터리)
- `src/main.cpp`, `src/ui/main_window.cpp`
- `tests/real_fixture.cpp`, `tests/real_backend.cpp`, `tests/value_codec.cpp`, `tests/real_smoke.cpp`

기존 저장소 파일이 git에서 untracked인 상태도 그대로 유지합니다. commit/push는 하지 않았습니다.
