# KCF Tool — v0.1

Linux C++17 / Qt6 Widgets 기반 standalone Tool입니다. GUI는 `ToolBackend`만 사용하며,
기본 `MockBackend`와 선택적인 실제 `KcfBackend`를 제공합니다.

## v0.1 기준

**KT-8 검증을 완료한 현재 상태를 v0.1로 지정합니다.** KT-0–KT-8의 기능과
KT-7.1 hardening을 포함하며, 실제 backend는 Framework R1–R5 + R4.1 + R2B.1을 기반으로 합니다.
Application/Element 탐색, Topic Echo, Parameter 편집, Service Call 및
다중 Application의 identity·stale 처리가 이 버전의 범위입니다.

검증 결과와 호환성·제한은 [v0.1 릴리스 노트](docs/V0_1_RELEASE_NOTES.md),
누적 작업 내역은 [Changelog](Changelog.md)를 참고하십시오.

## 빌드 및 실행

Mock 구성은 KCF 없이 빌드할 수 있습니다.

```sh
cmake -S . -B build -DKCF_TOOL_BUILD_TESTS=ON
cmake --build build -j
./build/kcf_tool --backend mock
ctest --test-dir build --output-on-failure
```

실제 backend에는 **R1–R5 + R4.1 + R2B.1** API/ABI를 포함하는 검증된 KCF revision이 필요합니다.
`KCF_SOURCE_DIR`에 임의 revision을 지정하면 호환성을 보장할 수 없습니다. 소스를 복사하지 않습니다.
현재 검증 대상은 로컬 `feature/introspection` 개발 상태이며, 배포용 정확한 commit/tag는 아직 확정하지 않았습니다.
Use a KCF revision containing R1-R5, R4.1 and R2B.1. Pin the exact Framework revision for deployment.

```sh
cmake -S . -B build-kt3 \
  -DKCF_SOURCE_DIR=/tmp/kcf-r1-introspection \
  -DKCF_TOOL_BUILD_TESTS=ON \
  -DCMAKE_PREFIX_PATH=/opt/Qt/6.9.3/gcc_64
cmake --build build-kt3 -j
./build-kt3/kcf_tool --backend kcf
ctest --test-dir build-kt3 --output-on-failure
```

Qt가 기본 검색 경로에 있으면 `CMAKE_PREFIX_PATH`는 생략합니다.
`-DKCF_TOOL_BUILD_GUI=OFF`로 Qt 없는 backend 전용 빌드도 가능합니다.
`KCF_SOURCE_DIR`를 생략한 빌드의 `--backend kcf`는 기존 disconnected stub입니다.
실제 통합 테스트는 하드웨어 없이 테스트용 프로세스, SHM 및 loopback UDP를 사용합니다.
자동 통합 테스트는 다른 KCF 프로세스가 없는 환경에서 실행하십시오.
일반 GUI 사용 시에는 탐색할 Application을 먼저 실행합니다.

## KT-8 검증 프로젝트 실행

독립 프로젝트는 현재 `/tmp/kcf_generic_validation`에 있습니다. Tool 저장소에
포함된 디렉터리가 아니므로 다른 환경에서는 해당 프로젝트를 별도로 준비하고
아래 경로를 조정해야 합니다. `/tmp`의 소스·빌드·로그는 임시 파일입니다.

### 빌드 및 자동 검증

```sh
cmake -S /tmp/kcf_generic_validation -B /tmp/kt8-build \
  -DKCF_SOURCE_DIR=/tmp/kcf-r1-introspection \
  -DKCF_TOOLS_SOURCE_DIR=/home/kssvm/workspace/kcf/kcf_tools \
  -DCMAKE_PREFIX_PATH=/opt/Qt/6.9.3/gcc_64
cmake --build /tmp/kt8-build -j4
ctest --test-dir /tmp/kt8-build --output-on-failure
```

CTest가 두 Application과 Standalone을 실행하고 Qt offscreen에서 실제 GUI를
조작합니다. 로컬 SHM, loopback UDP 및 프로세스 실행 권한이 필요합니다.
고정 endpoint 이름과 discovery 개수를 사용하는 테스트이므로 수동 실행 중인
검증 Application을 모두 종료한 후 실행하고, 기존 Tool 회귀와도 순차 실행합니다.
결과 로그는 `/tmp/kt8-build/Testing/Temporary/LastTest.log`, typed readback·watcher
증거는 테스트가 출력하는 `/tmp/kt8-evidence-*` 디렉터리에 남습니다.

### Application과 GUI 수동 실행

위 빌드를 완료한 후 각 명령을 별도 터미널에서 실행합니다. 실제 Tool GUI는
앞의 `build-kt3` 구성으로 빌드하고, 화면 표시가 가능한 데스크톱 세션에서 실행합니다.

```sh
# 터미널 1: Alpha — sensor / processor
/tmp/kt8-build/sensor_suite application /tmp/kt8-manual-alpha /validation
```

```sh
# 터미널 2: Beta — controller / actuator
/tmp/kt8-build/control_suite application /tmp/kt8-manual-beta /validation
```

```sh
# 터미널 3: Standalone
/tmp/kt8-build/diagnostic_node
```

```sh
# 터미널 4: Tool 저장소 디렉터리에서 실행
./build-kt3/kcf_tool --backend kcf
```

Applications에서 Refresh하면 `sensor_suite`, `control_suite`, `Standalone`
그룹이 표시됩니다. Element 상세에서 Topic을 열어 Start Echo, Parameter를 열어
편집 후 Apply, Service를 열어 입력 후 Call을 사용할 수 있습니다.

| 대상 | Topic | Parameter | Service |
| --- | --- | --- | --- |
| Alpha | `/validation/sensor/sample` | `/validation/sensor/config` | `/validation/processor/reset` |
| Beta | `/validation/control/state` | `/validation/control/gains` | `/validation/actuator/set_mode` |
| Standalone | `/validation/diagnostic/status` | `/validation/diagnostic/config` | 없음 |

같은 Application 이름의 추가 instance는 별도 터미널에서 다음과 같이 실행합니다.
endpoint prefix를 분리하여 기존 Publisher의 global SHM ownership과 충돌하지 않게 합니다.

```sh
/tmp/kt8-build/control_suite application /tmp/kt8-manual-beta2 /validation/second
```

종료할 때는 GUI를 닫고 각 Application/Standalone 터미널에서 Ctrl+C를 누릅니다.
남아 있는 GUI에서는 Refresh로 종료된 Runtime을 제거합니다. Endpoint 재생성이나
stale 오류 후에도 Refresh하고 새 endpoint를 선택해 접근합니다.

검증 결과: **KT-8 1/1, 기존 Tool 10/10, KCF-less GUI 3/3·backend 1/1,
Framework 회귀 모두 PASS**. Tool production 코드와 KCF core 변경 없이 검증했습니다.
세부 항목과 실행 범위는 [KT-8 결과 보고서](docs/KT8_GENERIC_CROSS_APPLICATION_VALIDATION.md)를 참고하십시오.

## 동작

- Applications 트리는 Supervisor identity별 Application과 소속 Runtime을 표시합니다.
- membership에 연결되지 않은 Runtime은 Standalone에 표시합니다. 이름 대신 PID + start_ticks로 구분합니다.
- Element를 더블클릭하거나 Enter로 열고, 상세 endpoint를 같은 방법으로 열면 기존 Echo/Editor/Call 화면으로 이동합니다.
- 수동 Refresh로 application/runtime/endpoint/service 목록을 갱신합니다. 사라진 세대의 선택은 해제됩니다.
- 실행 중인 유효 runtime 또는 supervisor가 있으면 Connected, 없으면 Disconnected입니다.
- backend는 Topic latest 읽기, Parameter Get/Set, Service Call을 제공합니다.
- 기존 화면에 Services 탭을 추가했습니다. Parameters에서는 scalar·fixed array를 조회하고 필드별로 편집합니다.
- Apply는 입력 검증 후 최신 값을 읽어 수정 항목을 반영하고, 성공 뒤 실제 값을 다시 조회합니다.
- 편집 중에는 Apply 또는 Revert 전까지 선택 변경·Refresh·창 닫기를 막습니다. 자동 쓰기는 없습니다.
- Services에서 request를 편집하고 Call하면 worker가 요청하고 GUI에 response를 표시합니다.
- Call 중에는 backend 조작을 잠시 비활성화하고 Echo를 중지합니다. GUI event loop는 계속 동작합니다.
- Timeout은 callback 미실행을 보장하지 않으며 Tool은 자동 재호출하지 않습니다.
- Topics에서 publisher를 선택하고 Start Echo를 누르면 100 ms마다 scalar/array 값을 표시합니다.
- Echo 동안 하나의 reader를 유지하며, R4.1의 SHM 재생성 감지를 사용합니다.
- Stop Echo, Topic 변경, Refresh, 창 닫기 또는 치명적 읽기 오류 시 polling을 중지합니다.
- 재생성 감지 후 Refresh와 Start Echo가 필요합니다. 첫 sample 대기는 자동 중지하지 않습니다.
- Dynamic Monitor API는 계속 `-ENOTSUP`입니다. graph/launch 확장은 미구현입니다.
- Mock의 시연 데이터와 기존 독립 테스트는 유지됩니다.
- 실제 Open 전후와 Parameter Set / Service Call 직전에 cached registration identity와 현재 registry를 비교합니다.
  사라지거나 바뀐 registration은 `-ESTALE`이며 Refresh 후 새 endpoint를 선택해야 합니다.
- 정상 시작한 Echo의 polling에는 registry 조회를 추가하지 않습니다. 이후 SHM 재생성은 기존 R4.1이 감지합니다.
- Parameter/Service float 입력은 finite 값만 허용합니다. NaN/Inf와 overflow 입력은 거부하며, 읽은 NaN/Inf는 관측용으로 표시할 수 있습니다.
- 실제 backend가 측정하지 않는 heartbeat/frequency와 sample 이전 sequence는 `N/A`로 표시합니다.
- registration 검증은 원자적 보장이 아닙니다. 특히 Service protocol에는 registration_id가 없으므로
  마지막 검증 직후 재등록되는 극단적인 race까지 막지는 못합니다. 발견한 stale metadata로는 Call하지 않습니다.

Current KCF introspection development compatibility: **SHM format = 3, Service protocol = 2**.
Supervisor와 child는 R2B.1을 포함한 동일 Framework revision으로 **rebuild + restart**해야 합니다.
child 외부의 SystemStatus observer는 `OpenForApplication(supervisor_pid, start_ticks)`로 대상을 지정합니다.

R2B.1 적용 후 서로 다른 이름 및 같은 이름의 Supervisor 동시 실행과
**KT-7 실제 다중 Application 검증은 PASS**입니다.
[KT-7 최초 보고서](docs/KT7_APPLICATION_ELEMENT_EXPLORER.md)의 FAIL은 R2B.1 적용 전의 역사적 기록입니다.
현재 Framework 검증 기록은 Framework checkout의 `examples/introspection/R2B1.md`를 참고하십시오.
[KT-7.1 보완·검증 보고서](docs/KT7_1_PRE_V01_HARDENING.md)를 참고하십시오.

[KT-6 Service Call UI 완료 보고서](docs/KT6_SERVICE_CALL_UI.md),
[KT-5 Parameter Viewer / Editor 완료 보고서](docs/KT5_PARAMETER_VIEWER_EDITOR.md),
[KT-4.1 persistent Echo 완료 보고서](docs/KT4_1_PERSISTENT_TOPIC_ECHO.md)를 참고하십시오.
실제 Echo에는 Framework R4.1 적용 버전이 필요합니다.
[KT-4 당시 구현·제한](docs/KT4_TOPIC_ECHO_UI.md)의 recreate 제한은 KT-4.1에서 해결했습니다.
[KT-3 구현·API·검증 결과](docs/KT3_REAL_KCF_BACKEND.md)를 참고하십시오.
이전 설계 문서는 당시의 분석 기록입니다:
[KT-1 외부 계약](docs/KCF_EXTERNAL_INTERFACE_REQUIREMENTS.md),
[KT-2 Gap Analysis](docs/KCF_INTERFACE_GAP_ANALYSIS.md),
[Framework Extensions](docs/KCF_REQUIRED_FRAMEWORK_EXTENSIONS.md).
