# KT-4.1 Persistent Topic Echo Result

## 구현

`StartTopicEcho(identity)` → 반복 `ReadTopicEcho(snapshot)` → `StopTopicEcho()` 계약을
ToolBackend/Real/Mock에 추가했습니다. GUI는 같은 API를 사용하며 기존 Topics 화면과 100 ms timer를 유지합니다.

KcfBackend는 하나의 session에 endpoint identity(PID/start ticks/registration ID), descriptor/type_id,
process lifetime pidfd, 실제 DynamicTopicReader를 보관합니다. 이름과 SHM incarnation은 해당 reader가
Open 때 바인딩합니다. Start는 이전 session을 먼저 닫고 metadata/GetType/Open이 모두 성공한 뒤
새 session을 저장합니다. 실패 시 부분적으로 열린 session은 RAII로 제거됩니다.

ReadTopicEcho는 저장된 reader의 ReadLatest와 cached descriptor decode만 수행합니다.
매 tick ListRuntimes/ListEndpoints/GetType/DynamicTopicReader::Open을 호출하지 않습니다.
R4.1 내부의 object identity 검증은 Framework가 수행합니다. Tool은 SHM 이름 인코딩이나 inode를 다루지 않습니다.

`-EAGAIN`은 KCF의 최초 미발행 sample 또는 bounded snapshot 경합을 의미하므로
기존 reader/timer를 유지하고 대기 상태를 표시합니다. `-ESTALE`, `-ENOENT`, `-EPROTOTYPE` 등
다른 오류는 session을 즉시 해제하고 오류를 그대로 GUI에 전달합니다. 실패 시 caller output은 보존됩니다.
비활성 Real/Mock session의 ReadTopicEcho는 `-EBADF`입니다.

GUI의 fatal error 경로는 timer를 중지하고 Start를 비활성화합니다. 재생성/제거에는
`Topic was removed or recreated. Refresh and start Echo again.`을 표시합니다.
Refresh 후 현재 metadata를 확인하고 사용자가 Start해야 하며 자동 reopen은 없습니다.
Stop은 마지막 화면 값을 유지하지만 추가 read는 하지 않습니다. Topic 선택 변경은 이전 reader를 닫고
표를 비웁니다. 새 Topic의 Start는 새 reader를 엽니다. Refresh/창 닫기/소멸도 session을 해제합니다.
선택 시 수행하는 일회성 지원 여부 검사는 timer 경로 밖에 있습니다.

KT-3 `ReadTopicLatest(name/identity)`는 항상 one-shot inspection으로 유지합니다.
활성 Echo와 독립적이며 one-shot 읽기가 Echo session을 교체하거나 닫지 않습니다.
이전 `PrepareTopicRead`/`CloseTopicRead` 이름은 Start/Stop 호환 별칭으로 남겼습니다.
반복 Echo 읽기는 반드시 `ReadTopicEcho`를 사용해야 합니다.

Mock도 active identity를 저장하고 Read에서 해당 Topic의 sample을 반환하며 Stop/Refresh에서 지웁니다.
기존 Parameter 동작과 Dynamic Monitor의 `-ENOTSUP` 계약은 유지합니다.

## 검증

기존 외부 KCF/Qt 구성 `build-kt3`에서 build 및 CTest를 실행했습니다.
실제 연결 대상은 `/tmp/kcf-r1-introspection`, `feature/introspection`, **R4.1 적용 버전**입니다.

```sh
cmake -S . -B build-kt3 -DKCF_SOURCE_DIR=/tmp/kcf-r1-introspection \
  -DKCF_TOOL_BUILD_TESTS=ON -DCMAKE_PREFIX_PATH=/opt/Qt/6.9.3/gcc_64
cmake --build build-kt3 -j4
ctest --test-dir build-kt3 --output-on-failure
```

별도 KCF/Qt 없는 backend build 및 contract test도 확인했습니다.
기존 준비 상태 테스트는 service 발견만 기다리던 조건에서 endpoint/Parameter 목록까지 기다리도록
수정했습니다. GUI 테스트도 첫 sample 전에는 timer가 살아 있는 것을 확인하고 표가 채워질 때까지 기다립니다.

| 항목 | 결과 |
|---|---|
| Build | 외부 KCF + Qt PASS; KCF/Qt 없는 backend build/test 1/1 PASS |
| Persistent Reader | Start 때 한 번 Open, session 내부 reader 유지; 재생성 stale 결과로 검증 |
| Start | 기존 session 제거 후 commit, invalid identity 실패 시 inactive PASS |
| Read | 반복 20회 성공, cached reader/descriptor 사용; one-shot과 독립 PASS |
| Stop | idempotent, 이후 -EBADF, 재Start/Refresh/close 해제 PASS |
| Same PID / Same Type Recreate | PID/start ticks/type_id 동일, 새 registration ID, A→B 사이 old session -ESTALE PASS |
| R4.1 -ESTALE | old output A 보존, 다음 Echo Read -EBADF PASS |
| Automatic Stop | GUI timer 중지, 오류 표시, Refresh 전 Start 비활성 PASS |
| Refresh / Reopen | 새 endpoint 발견 후 B 읽기 PASS |
| Topic Switch | 실제 standalone/supervised Topic A/B 전환 및 Mock/GUI 전환 PASS |
| MockBackend | 같은 session API, A/B identity 보존, Stop/Refresh 상태 해제 PASS |
| Real KCF | 재생성, Publisher 종료, unlink, 최초 미발행 -EAGAIN 대기/후속 payload 표시 PASS |
| Qt Smoke | Mock 및 real offscreen PASS; 기존 5개 탭 유지 |
| KCF Core | NOT MODIFIED |
| mecanum | NOT MODIFIED |
| Dynamic Monitor | NOT IMPLEMENTED / -ENOTSUP |
| KT-4 Final | PASS |
| Result | PASS |

테스트 구성은 기존 KT-3 real backend/codec, backend contract, Mock Qt 및 real Qt smoke의 5개입니다.
최종 CTest **5/5 PASS** (2.23초). 로그는 `build-kt3/Testing/Temporary/LastTest.log`입니다.
외부 Framework source 132개 파일은 R4.1 기준 SHA256와 모두 일치합니다.
실제 GUI recreate 테스트는 같은 byte 값으로 재생성해도 timer가 중지되는 것을 확인합니다.
backend recreate 테스트는 A와 B가 다른 경우 B를 old session 성공값으로 반환하지 않는 것을 확인합니다.
추가 Open 계수 instrumentation이나 Framework 테스트 수정 없이 lifecycle 결과를 검증했습니다.

## Problems / 범위

현재 확인된 실패 없음. 동작은 동기식 단일 Echo session이며, GUI 100 ms polling입니다.
Framework callback monitor, background subscription, Parameter/Service UI 변경은 없습니다.
R4.1의 object identity check는 시점 검사이며 concurrent unlink와 read/write 전체를 원자적으로
묶는 계약은 아닙니다. 기존 Linux pidfd 및 Framework observer isolation 제한도 유지됩니다.
시각적 레이아웃의 수동 데스크톱 검사는 offscreen 테스트 범위 밖입니다.

## Modified Files

- `include/kcf_tool/backend/tool_backend.hpp`
- `include/kcf_tool/backend/kcf_backend.hpp`
- `include/kcf_tool/backend/mock_backend.hpp`
- `include/kcf_tool/ui/main_window.hpp`
- `src/backend/kcf_backend.cpp`
- `src/backend/kcf_backend_stub.cpp`
- `src/backend/mock_backend.cpp`
- `src/ui/main_window.cpp`
- `tests/backend_contract.cpp`
- `tests/real_backend.cpp`
- `tests/real_fixture.cpp`
- `tests/real_smoke.cpp`
- `README.md`
- `docs/KT4_TOPIC_ECHO_UI.md` (후속 해결 결과 링크)
- `docs/KT4_1_PERSISTENT_TOPIC_ECHO.md` (신규)

기존 변경과 untracked 상태를 보존했습니다. commit/push는 하지 않았습니다.
