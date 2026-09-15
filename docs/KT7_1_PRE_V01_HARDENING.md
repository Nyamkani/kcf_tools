# KT-7.1 Pre-v0.1 Hardening Result

## Cached registration 검증

KcfBackend 내부 공통 helper가 기존 cached record와 현재 공개 introspection registry를 비교합니다.
Topic/Parameter는 `ListEndpoints(runtime)`, Service는 `ListServices(runtime)`를 사용합니다.
Runtime의 PID/start_ticks 검증은 해당 공개 API가 수행하며, cached identity와 저장한 RuntimeInfo도 비교합니다.

- Topic/Parameter: registration_id, kind, role, name, type_id, payload_size.
- Service: registration_id, name, port, service_id, request/response type_id와 size.
- 없거나 바뀐 registration과 종료된 runtime은 `-ESTALE`입니다. 나머지 discovery 오류도 접근을 중단하고 전달합니다.
- backend cache 자체에 없는 identity는 기존대로 `-ENOENT`입니다.

실제 접근 순서:

- StartTopicEcho / identity ReadTopicLatest: registration 검증 → descriptor → Open → registration 재검증 → 사용.
- Parameter Get: registration 검증 → descriptor → Open → registration 재검증 → Get.
- Parameter Set: descriptor/입력 Encode → registration 검증 → Open → seed Get → Encode overlay → registration 재검증 → Set.
- Service Call: descriptor/입력 Encode → registration 검증 → Open → registration 재검증 → Call.

ValidateParameter/ValidateService의 기존 descriptor 기반 candidate 검증에는 endpoint/service discovery를 추가하지 않았습니다.
정상 시작한 persistent Echo의 tick에도 registry 조회를 추가하지 않았습니다.

KT-7.1은 Refresh 이후 registration 교체를 검출합니다. R4.1은 열린 handle이 가리키는 SHM의 unlink/recreation을 검출합니다.
두 검증은 시점별 검사이며 원자적인 registration/storage transaction을 제공하지 않습니다.
특히 UDP Service protocol에는 registration_id가 없으므로 최종 검증 직후 같은 process가 같은 port/service_id로
재등록하는 극단적인 race까지 차단한다고 보장하지 않습니다. 계약은 **발견한 stale cached service metadata로 Call하지 않는다**입니다.
Service protocol은 변경하지 않았습니다.

## 입력과 표시

- FLOAT32/FLOAT64 Encode는 finite 값만 허용합니다. NaN/Inf 표기와 overflow는 `-ERANGE`로 거부합니다.
- Decode는 실제 payload의 NaN/Inf를 계속 표시할 수 있습니다.
- Element heartbeat, Topic frequency/sequence에 명시적인 availability flag를 추가했습니다. 숫자 0을 sentinel로 쓰지 않습니다.
- 실제 backend의 미측정 heartbeat/frequency와 sample 이전 sequence는 `N/A`입니다. 실제 Echo sample 수신 후 sequence를 표시합니다.
- Mock이 제공하는 값은 숫자로 계속 표시합니다. 유효한 heartbeat 0도 값이 있는 것으로 구분합니다.
- stale Parameter는 Revert → 전체 Refresh → 새 endpoint 선택이 필요합니다. Refresh Value만으로 새 registration에 넘어가지 않습니다.

## 검증

`registration_hardening`은 실제 ProcessRuntime 안에서 Topic/Parameter/Service를 같은 process/name/type으로 재등록합니다.
old identity의 접근 거부, Refresh 후 새 identity 접근, 값과 handler 호출 횟수 보존을 확인합니다.
테스트 전용 public API link wrapping으로 Open 직후와 seed Get 직후 재등록도 발생시킵니다.
Parameter client registration만 바꾸고 SHM은 유지하는 사례도 검증하여 R4.1과 별도로 KT-7.1 검사가 동작함을 확인합니다.
Framework 소스나 storage internals에 hook을 넣지 않습니다.

FLOAT32/FLOAT64와 float array에 `nan`, `-nan`, `inf`, `-inf`, `NaN`, `Infinity`, `1e9999`를 입력하여
validation/Encode 실패 및 실제 storage Open/Service Open 미실행을 확인했습니다.
GUI 테스트는 잘못된 float 입력에서 Set/Call backend 메서드가 호출되지 않는지 확인합니다.
codec 테스트는 실패 시 output 보존과 실제 NaN/Inf payload의 Decode도 확인합니다.
Real GUI의 N/A → sample sequence 표시와 Mock 숫자 표시를 검증했습니다.

| 항목 | 결과 |
|---|---|
| Build | PASS: 별도 clean build, C++17 / Qt6 |
| Topic Registration Revalidation | PASS: stale cached identity 및 Open 사이 교체 거부 |
| Parameter Registration Revalidation | PASS: old identity Get/Set 거부, 새 identity 정상 접근 |
| Stale Parameter Set Protection | PASS: old/new storage 값 보존; 동일 SHM의 client registration 교체도 거부 |
| Service Registration Revalidation | PASS: 동일 process/name/type/port 재등록, handler 추가 호출 없음 |
| Service Race Limitation | 최종 검사와 실제 UDP 처리 사이의 원자적 보장 없음 |
| Finite Float Input | PASS: finite만 Encode, non-finite Decode 유지 |
| Unavailable Metrics / N/A | PASS: 명시적 flag, Real N/A 및 Mock 숫자 표시 |
| README | KT-7 최종 PASS, R2B.1 compatibility 및 배포 revision 고정 필요 명시 |
| Changelog | KT-7.1 추가, 로컬 Framework 파일을 public 링크가 아닌 plain text reference로 변경 |
| Regression | PASS: 기존 9개 + registration_hardening = 10/10 |
| KCF-less GUI | PASS: 3/3 |
| KCF-less Backend | PASS: 1/1 |
| Strict compile | PASS: 변경 backend/codec와 hardening test에 -Wall -Wextra -Wpedantic -Werror |
| KCF Core | NOT MODIFIED |
| mecanum | NOT MODIFIED |
| SHM Format | 3 |
| Service Protocol | 2 |
| Problems | 미해결 테스트 실패 없음. 위 시점별 검증의 race 한계는 유지됨 |
| Result | PASS |

검증 build directory: `/tmp/kt71-build`, `/tmp/kt71-no-kcf-gui`, `/tmp/kt71-no-kcf-backend`.
각 디렉터리에서 `ctest --output-on-failure`로 등록된 테스트를 실행할 수 있습니다.
Framework는 R1–R5 + R4.1 + R2B.1을 포함하는 로컬 개발 상태로 검증했으며, 확정되지 않은 SHA/tag를 기재하지 않았습니다.

## Modified Files

- `CMakeLists.txt`
- `README.md`, `Changelog.md`
- `include/kcf_tool/model/element_info.hpp`, `include/kcf_tool/model/topic_info.hpp`
- `src/backend/kcf_backend.cpp`, `src/backend/value_codec.cpp`, `src/backend/mock_backend.cpp`
- `src/ui/main_window.cpp`
- `tests/registration_hardening.cpp`, `tests/value_codec.cpp`
- `tests/parameter_smoke.cpp`, `tests/service_smoke.cpp`, `tests/real_smoke.cpp`, `tests/smoke.cpp`
- `docs/KT7_1_PRE_V01_HARDENING.md`

Commit / Push: NOT PERFORMED.
