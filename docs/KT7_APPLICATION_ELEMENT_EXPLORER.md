# KT-7 Generic Application / Element Explorer Result

GUI 구현과 실행 가능한 회귀 테스트는 PASS입니다. 전체 요청 기준 Result는 **FAIL**입니다.
현재 외부 KCF Bringup의 단일 SystemStatus writer 제약 때문에 실제 Supervisor Application
두 개의 동시 실행 항목을 충족할 수 없습니다. KCF core는 수정하지 않았습니다.

## 구현

- **Application Discovery:** 기존 `GetApplications()` 결과로 Applications 트리를 구성합니다.
  Application 이름, Supervisor PID/start_ticks, 상태, managed count, revision을 표시합니다.
- **Membership Mapping:** 기존 KcfBackend가 `ListSupervisors()` membership과 `ListRuntimes()`를
  PID + start_ticks로 연결한 `ElementInfo.supervisor`를 사용합니다. Application 이름은 그룹 키가 아닙니다.
  살아 있는 runtime만 자식으로 표시하며, 종료된 runtime은 Refresh에서 제거됩니다.
- **Standalone:** 현재 Supervisor identity에 연결되지 않은 runtime을 별도 그룹으로 표시합니다.
  실행 mode가 SUPERVISED여도 membership을 찾지 못하면 이 그룹에 포함합니다.
- **Element Detail:** 이름, executable, mode, state, error 및 runtime/supervisor identity를 표시합니다.
  기존 Heartbeat 항목은 유지했습니다. R1 RuntimeInfo에는 실시간 heartbeat가 없으므로 실제 backend의
  해당 숫자는 관측된 heartbeat를 의미하지 않습니다.
- **Endpoint/Service Lists:** 선택 runtime의 identity로 Topic, Parameter, Service를 필터링합니다.
  이름, 역할(Topic/Parameter), registration ID를 표시합니다.
- **Existing Topic/Parameter/Service Integration:** endpoint를 더블클릭하거나 Enter로 활성화하면
  현재 snapshot의 정확한 endpoint 행을 기존 Topics/Parameters/Services 탭에서 선택합니다.
  새 Echo/Editor/Call 구현이나 backend API를 추가하지 않았습니다.
- **Reset Generation:** runtime PID/start_ticks와 endpoint registration identity로 선택을 보존합니다.
  runtime 교체/종료 시 stale selection을 해제하고, 후속 Refresh에서 다른 endpoint를 자동 선택하지 않습니다.
- **Generic / No Hardcoding:** GUI discovery/분류/탐색에 application-specific 이름이나 분기가 없습니다.
  기존 Launch의 특정 application 예시를 일반적인 미구현 안내로 바꿨습니다.
  이전 Mock 시연 데이터는 그대로 두고 synthetic identity와 Supervisor 연결만 보완했습니다.
- Refresh는 수동입니다. Parameter 편집 중에는 기존 Apply/Revert 보호를 유지하고,
  Service Call 중에는 기존 backend 직렬화 보호를 유지합니다.
- Graph, Launch 및 신규 Topic/Parameter/Service 기능은 구현하지 않았습니다.

## 검증과 미충족 항목

`kcf_tool_explorer_identity`는 KCF 없이 실행되는 Qt metadata 테스트입니다.
동일 이름의 서로 다른 Supervisor 두 개, 자식 runtime들, membership generation이 맞지 않는
Standalone을 동시에 표시합니다. 목록 순서 변경, PID가 같고 start_ticks만 변경되는 경우,
후속 Refresh의 stale selection 및 backend 오류 시 캐시 제거를 확인합니다.
이 테스트는 실제 Supervisor 두 개의 동시 실행 증거가 아닙니다.

`kcf_tool_explorer_smoke`는 외부 KCF의 실제 Bringup/ProcessRuntime을 사용합니다.
Application A(자식 1개) + Standalone 실행, runtime별 endpoint 목록과 기존 화면 연결,
ERROR → RequestReset → 새 runtime 세대, Standalone 종료/동일 이름 재생성,
Application A 종료 후 Application B(자식 2개) 발견과 종료를 검증합니다.
초기화 경합을 피하기 위해 RUNNING 전환을 기다린 뒤 기존 계약대로 ERROR 상태에서 Reset합니다.
fixture의 signal handler는 lock-free flag만 설정하며 별도 일반 thread에서 RequestReset을 호출합니다.

최초 실제 동시 실행 시도 결과:

```text
A: [Supervisor] INITIALIZING -> RUNNING
B: [Supervisor] SystemStatus Create error=-17
```

외부 `kcf/include/kcf/system/system_status_channel.hpp`의 `SystemStatusPublisher::Create()`는
고정 `SYSTEM_STATUS_STORAGE`를 사용합니다. `Bringup::Setup()`은 두 번째 Create의 `-EEXIST`에서
introspection 등록 전에 종료합니다. 따라서 Tool이 두 번째 Application을 발견할 metadata 자체가 없습니다.
이 제약을 우회하려고 core를 수정하거나 SystemStatus를 강제로 unlink하거나 테스트에서 storage를
바꿔치기하지 않았습니다. 최종 live 테스트는 서로 다른 두 Application을 **순차 실행**합니다.

검증 명령:

```sh
cmake --build build-kt3 -j4
ctest --test-dir build-kt3 -R kcf_tool_explorer_smoke --repeat until-fail:5 --output-on-failure
ctest --test-dir build-kt3 --output-on-failure --timeout 120 -j1
cmake --build build-kt3-mock -j4
ctest --test-dir build-kt3-mock --output-on-failure
cmake -S . -B build-kt7-no-kcf -DKCF_TOOL_BUILD_GUI=ON -DKCF_TOOL_BUILD_TESTS=ON \
  -DCMAKE_PREFIX_PATH=/opt/Qt/6.9.3/gcc_64
cmake --build build-kt7-no-kcf -j4
ctest --test-dir build-kt7-no-kcf --output-on-failure
```

Regression: 기존 KT-3–KT-6 포함 실제 연결 구성 **9/9 PASS** (5.66초),
KCF 없는 GUI 구성 **3/3 PASS**, KCF 없는 backend-only 구성 **1/1 PASS**.
초기화 경합 수정 후 실제 Explorer/Reset 테스트 **5회 연속 PASS**.
위 PASS는 지원되는 경로의 회귀 결과이며, 실제 두 Bringup 동시 실행 요구의 PASS를 의미하지 않습니다.

## 변경 파일

- `.gitignore`: 새 KCF 없는 GUI 검증 build directory 제외.
- `CMakeLists.txt`: Explorer live/identity 테스트 등록.
- `README.md`: KT-7 사용법과 acceptance 제한.
- `include/kcf_tool/ui/main_window.hpp`: Explorer widget/snapshot state.
- `src/ui/main_window.cpp`: hierarchy, detail lists, 기존 화면 연결, identity 기반 선택.
- `src/backend/mock_backend.cpp`: 기존 시연 데이터의 runtime/supervisor identity 연결.
- `tests/smoke.cpp`: Applications 탭 이름 회귀 기대값.
- `tests/real_fixture.cpp`: 임의 Application 이름/자식 구성 및 테스트용 Reset trigger.
- `tests/explorer_identity.cpp`: KCF 독립 metadata/identity 테스트.
- `tests/explorer_smoke.cpp`: 실제 runtime/Reset/navigation 테스트.
- `docs/KT7_APPLICATION_ELEMENT_EXPLORER.md`: 본 보고서.

KCF Core: **NOT MODIFIED** — 이전 manifest의 132개 파일 SHA-256 동일.

mecanum: **NOT MODIFIED**. 기존 사용자 변경을 보존했습니다.

Commit / Push: **NOT PERFORMED**.

Result: **FAIL — 실제 Application 두 개 동시 실행 미충족.**
