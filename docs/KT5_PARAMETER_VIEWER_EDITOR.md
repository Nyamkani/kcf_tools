# KT-5 Parameter Viewer / Editor Result

## 사용

기존 Parameters 탭에 Field/Type/Current/Edit 표와 Refresh Value, Apply, Revert를 연결했습니다.
선택하면 실제 current value를 읽고 이름, OWNER/CLIENT, PID/start ticks, registration ID,
diagnostic type name, type_id, payload size를 표시합니다. OWNER 항목을 먼저 나열하되 동일 이름의
CLIENT를 버리지 않습니다. 선택과 접근에는 EndpointIdentity를 사용합니다.

scalar 및 fixed primitive array의 원소를 텍스트로 편집합니다. 배열은 `values[0]` 형태로 펼칩니다.
단일 scalar는 기존 editor를 유지합니다. bool은 true/false, 정수는 각 bit-width/signed 범위,
float는 KT-3 parse 규칙을 따릅니다. 64-bit 정수도 문자열로 유지하여 정밀도를 잃지 않습니다.
UI에서 배열 개수나 field 이름/type 자체는 변경할 수 없습니다.

입력은 즉시 쓰지 않습니다. Apply 버튼 또는 단일 scalar editor의 명시적 Enter만 적용 요청입니다.
Typing/focus loss/timer/retry에 의한 Set은 없습니다. 자동 Parameter watch도 없습니다.

## Dirty state와 오류

편집 상태에서는 Apply 또는 Revert 전까지 다른 Parameter 선택, Refresh Value, 전체 Refresh,
창 닫기를 막고 안내합니다. Revert는 마지막으로 Get한 편집 baseline으로 되돌리며 **storage에 쓰지 않습니다**.
외부 변경을 보려면 Revert 후 Refresh Value를 사용합니다.

invalid input은 편집값과 마지막 current 표시를 유지하며 오류를 표시합니다.
Get/Set의 stale/type/runtime 오류는 Apply를 비활성화하고 current value가 확인되지 않았음을 표시합니다.
`-ESTALE`에는 `Parameter was recreated. Revert edits and Refresh before applying.`을 표시합니다.
오류 뒤 자동 reopen + Set은 하지 않습니다. 편집값을 Revert하고 수동 Refresh로 복구합니다.

`type_id == 0`은 `Type descriptor unavailable`이며 Get/editor/Apply를 비활성화합니다.
readonly model도 편집하지 못합니다. CLIENT만 남은 경우에도 실제 Open/Get 성공을 확인하기 전에는
쓰기 가능한 editor를 활성화하지 않습니다. metadata의 writable 표시가 성공한 storage 접근을 대체하지 않습니다.

## Apply 경로

1. 완전한 편집 DataSnapshot을 `ToolBackend::ValidateParameter`로 검사합니다.
2. 검증 실패 시 `SetParameter`를 호출하지 않습니다.
3. 성공하면 identity 기반 Get으로 최신 전체 값을 다시 읽습니다.
4. schema/type를 확인하고 사용자가 바꾼 scalar 또는 배열 원소만 overlay합니다.
5. 기존 `KcfBackend::SetParameter`로 전달합니다.
6. Set 성공 뒤 다시 Get한 실제 값을 표시하고 dirty를 해제합니다.

ValidateParameter는 기존 KT-3 TypeDescriptor/Encode 로직을 재사용하며 Parameter storage를
Open/Set하지 않습니다. GUI는 FieldValue/DataSnapshot만 전달하며 bytes serializer를 추가하지 않았습니다.
KcfBackend의 기존 Set 경로는 한 DynamicParameterClient를 Open → seed Get → encode → Set까지
유지하므로 R4.1 identity 검증을 우회하지 않습니다. padding/비필드 bytes 보존도 기존 계약을 유지합니다.

**Parameter Set은 whole-value replacement입니다.** 최신 Get과 수정 항목 overlay는 그 전에 발생한
다른 writer의 미편집 field 변경을 보존하지만, Get/Set 사이의 추가 concurrent write까지 원자적으로
병합하거나 CAS로 보호하지는 않습니다. Set 성공 후 readback이 실패하면 Set 자체는 성공했지만
현재 값이 확인되지 않았음을 따로 표시하며 다시 쓰지 않습니다.

GUI는 KCF class, TypeDescriptor, SHM API를 사용하지 않습니다.
`GUI → ToolBackend → KcfBackend/MockBackend` 경계와 Qt-independent model을 유지합니다.

## 테스트와 결과

실제 owner fixture의 Config에 bool/int32/uint8/int8/float32 및 float 배열을 사용했습니다.
기존 value codec 회귀는 11개 primitive와 fixed array, extrema 및 malformed schema를 검증합니다.
Mock은 기존 데이터/readonly 계약을 유지하면서 identity 기반 Get/Validate/Set을 추가했습니다.

새 `kcf_tool_parameter_smoke`는 실제 Qt UI와 KcfBackend를 사용합니다.
ToolBackend decorator로 Set 호출 횟수를 세어 invalid input이 backend Set 이전에 차단됨을 검사합니다.
테스트에만 public DynamicParameterClient Get/Set의 link wrapping을 사용하여:

- 성공한 Set 직후 typed client가 값을 바꾸면 GUI가 재Get 결과를 표시하는지 확인합니다.
- 실제 Set client의 Open/seed Get 이후 같은 PID/type/name으로 owner를 재생성하고,
  이어지는 Set이 -ESTALE인지 확인합니다.

Framework 코드/테스트를 변경하거나 physical storage에 접근하는 hook은 없습니다.
old typed client와 새 owner 값을 Service fixture로 조회하여 stale Set이 양쪽 모두를 변경하지 않았음을 확인합니다.
이 Service는 테스트 확인용이며 GUI에 Service 기능을 추가하지 않았습니다.

| 항목 | 결과 |
|---|---|
| Build | 외부 KCF + Qt build PASS; KCF/Qt 없는 backend build/test PASS |
| Parameter Selection | identity 유지, OWNER 우선, dirty 선택 변경 차단 PASS |
| Metadata | role/runtime/diagnostic/type_id/size 표시 PASS |
| Get | 선택/Refresh Value에서 실제 current 읽기 PASS |
| Field Decode | 기존 KT-3 11 primitive codec + 실제 scalar/array 표시 PASS |
| Edit | field/array 원소 편집, 명시적 Apply, Revert 무쓰기 PASS |
| Input Validation | uint8=300, int8=-200, float=abc, bool=yes → Set 호출 0회 PASS |
| Apply | fresh Get + 수정 원소만 overlay; 외부 미편집 field/array 원소 보존 PASS |
| Post-Set Re-read | Set 후 typed writer의 변경값 표시, dirty 해제 PASS |
| Watcher Notification | 실제 typed watcher가 dynamic Set 통지 수신 PASS |
| R4.1 -ESTALE | old/new 값 66/88 유지, 후보 99 미적용, UI Refresh required PASS |
| External Update / Refresh | typed client 변경 후 수동 Refresh Value 표시 PASS |
| MockBackend | 동일 UI Get/Apply/validation/Revert/readonly PASS |
| Real KCF | 실제 Parameter Get/Set, owner 종료 오류 및 discovery 제거 PASS |
| Backend Boundary | GUI → ToolBackend only |
| Qt Smoke | 기존 Echo/Mock/real 및 신규 Parameter offscreen PASS |
| KCF Core | NOT MODIFIED; R4.1 source 132개 SHA256 일치 |
| mecanum | NOT MODIFIED; 기존 사용자 변경 보존 |
| Automatic Parameter Watch | NOT IMPLEMENTED |
| Result | PASS |

기존 구성 `build-kt3`에 새 테스트를 추가했습니다. 실행:

```sh
cmake --build build-kt3 -j4
ctest --test-dir build-kt3 --output-on-failure
```

KCF 없는 `build-kt3-mock`의 backend contract는 1/1 PASS입니다.
최종 전체 CTest는 **6/6 PASS** (2.54초)이며 로그는
`build-kt3/Testing/Temporary/LastTest.log`에 있습니다.
변경 backend는 -Wall/-Wextra/-Wpedantic/-Werror 구문 검사도 통과했습니다.

Problems: 현재 확인된 실패 없음. 최초 새 테스트의 fixture가 닫힌 Service를 잘못 재시작한 문제를
수정했습니다. 최종 fixture는 Service를 유지하고 client lifetime 변경만 테스트용 mutex로 보호합니다.
whole-value replacement 및 R4.1 namespace 시점 검사의 경합 제한은 유지됩니다.
수동 데스크톱 시각 검사는 offscreen 검증 범위 밖입니다.

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
- `tests/real_fixture.cpp`
- `tests/parameter_smoke.cpp` (신규)
- `docs/KT5_PARAMETER_VIEWER_EDITOR.md` (신규)

기존 변경/untracked 상태를 유지했습니다. commit/push는 하지 않았습니다.
