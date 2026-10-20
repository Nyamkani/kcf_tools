# KCF Tool — v0.13

Linux C++17 / Qt6 Widgets 기반 standalone Tool입니다. GUI는 `ToolBackend`만 사용하며,
기본 `MockBackend`와 선택적인 실제 `KcfBackend`를 제공합니다.

## v0.13 기준

현재 버전은 **v0.13** (CMake **0.13.0**)입니다. 기존 Remote Discovery와 Local 기능을
유지하며 다음 read-only Network 관측을 추가합니다.

- 기존 Observer의 `PeerDiscovery`와 공개 `NetworkDiagnosticsClient`로 Gateway를 조회합니다.
- Gateway별 Route identity/session, 상태와 9개 Counter를 보존합니다.
- Counter Delta 기반 Network Send/Receive Hz, Proxy Publish Hz, Local Skipped/s를 표시합니다.
- Remote Topic Detail에 Diagnostics를 표시하고, 양쪽 Gateway에서 검증된 ACTIVE Route만
  Publisher → Subscriber 방향의 Network Graph Edge로 연결합니다.

Framework 기준은 **dev v5.3.0, F-ND5B 완료 미커밋 working tree**입니다.
이 상태를 식별하는 공개 commit SHA/tag를 지정하지 않습니다. Diagnostics API가 없는
구형 Network Framework는 Discovery-only 관측을 유지하며, Network API도 없으면 Local-only,
Framework를 지정하지 않으면 Mock/stub를 제공합니다. Tool은 forwarding이나 Remote 쓰기/호출을 하지 않습니다.

기존 KT-ND3 기록은 현재 Framework 구성의 Tool **19/19**, 구형 Framework **18/18**,
Mock/stub **6/6**, 구형 Explorer 반복 **10/10 PASS**입니다. 이는 당시 Diagnostics working tree의
실행 기록이며, 이번 v0.13 문서 작업에서 F-ND5B 소스로 재테스트한 결과가 아닙니다.
KT-ND4 당시 남아 있던 `network_backend`의 5.2.0 고정 기대값은 **KT-ND5A에서 정합화·재검증을 완료**했습니다.
현재 테스트는 `DiscoveryConfig.profile.framework`의 major/minor/patch와 실제 관측 버전을 비교하며,
NetworkProtocolVersion **1.0** 검증은 유지합니다. KT-ND5A에서 v5.3 **19/19 PASS (51.51초)**,
v5.2 Discovery-only **18/18 PASS (29.03초)**, Mock/stub **6/6 PASS (2.14초)**를 확인했습니다.
세 구성 모두 최초 실행 PASS이며, 이번 KT-ND5B 문서 정정에서 테스트를 재실행한 결과는 아닙니다.
Virtual LAN + Qt offscreen E2E는 PASS, 물리 LAN과 실제 데스크톱 GUI 수동 검증은 미실행입니다.
Framework 최초 7/8 및 Topic·Conflict/Action·Explorer의 간헐적 실패/재실행 이력은
[v0.13 릴리스 노트](docs/V0_13_RELEASE_NOTES.md)에 구분합니다.

[Network 계약](docs/NETWORK_OBSERVATION.md#v013-network-diagnostics-계약),
[Verified Route Graph](docs/GRAPH.md#v013-verified-network-route-graph),
[Changelog](Changelog.md)를 참고하십시오. 이번 작업은 commit/push/tag/GitHub Release를 생성하지 않습니다.

## Historical — v0.12 기준

아래의 “현재/이번”은 v0.12 당시를 뜻합니다. SHA·날짜·검증 수치는 당시 기록으로 보존합니다.

**당시 버전은 Graph·Remote Discovery·읽기 전용 Remote 상세를 포함한 v0.12였습니다.**
v0.11의 Application/Element 탐색, Topic Echo, Parameter 편집, Service Call 및
identity·stale 처리를 유지합니다. Network 관측은 공개 Network API가 있는 Framework
checkout으로 빌드할 때 제공하며, Remote 쓰기·호출이나 forwarding 기능은 포함하지 않습니다.

2026-09-29 미커밋 Network working tree에서 수행한 과거 자동 회귀는 KCF + Qt **18/18 PASS**, Framework 없는 Mock/stub + Qt
**6/6 PASS**입니다. Network 검증은 loopback 및 Qt offscreen 기준이며 실제 물리 LAN과
데스크톱 수동 조작은 미검증입니다. v0.11의 사용자 Echo 수동 확인과 구분합니다.

검증 결과와 호환성·제한은 [v0.12 릴리스 노트](docs/V0_12_RELEASE_NOTES.md),
누적 작업 내역은 [Changelog](Changelog.md)를 참고하십시오.
[v0.1](docs/V0_1_RELEASE_NOTES.md)과 [v0.11](docs/V0_11_RELEASE_NOTES.md)은 기존 개발 이력으로 보존합니다.
당시 Tool 기준은 **kcf_tools main / v0.12**입니다.
`f4fb4471fc0b03b24663eb20eede8d8491fd87f6`는 v0.12 기능 최초 반영 커밋입니다.
Git tag 및 GitHub Release 생성 여부는 이번 작업에서 확인하지 않았습니다.

당시 d41fc2f 최종 검증은 수정 없는 독립 Framework 소스에서 새 configure/build 후
**KCF + Qt 18/18 PASS**, **Mock/stub + Qt 6/6 PASS**입니다. 실제 FrameworkVersion
5.2.0과 NetworkProtocolVersion 1.0을 검증했습니다. 아래 과거 결과와 구분합니다.

과거 2026-09-29 후속 검증에서는 공개 KCF v5.2 `0a3f2a6…`의 수정 없는 Git 추출본으로
새 configure/build 후 **KCF + Qt 18/18**, **Mock/stub + Qt 6/6 PASS**를 확인했습니다.
과거 working tree 결과와 별도이며, 검증 환경과 로그는 [Network 검증 기록](docs/NETWORK_OBSERVATION.md)을 따릅니다.

## 빌드 및 실행

Mock 구성은 KCF 없이 빌드할 수 있습니다.

```sh
cmake -S . -B build -DKCF_TOOL_BUILD_TESTS=ON
cmake --build build -j
./build/kcf_tool --backend mock
ctest --test-dir build --output-on-failure
```

현재 v0.13 Diagnostics 빌드는 **KCF v5.3.0 dev working tree (F-ND5B 완료)**를
`KCF_SOURCE_DIR`로 참조합니다. 소스를 복사하지 않으며 HEAD만으로 미커밋 변경을
식별할 수 없으므로 `git status`와 적용된 API도 확인합니다. 새 빌드 디렉터리를 사용합니다.

Historical: v0.12의 공개 Network 빌드 기준은 KCF v5.2
`d41fc2fc71d3b60b0475035ec56ac32b92ffb849`였습니다. 아래의 과거 검증 설명은 당시 기준입니다.

Local v0.11의 과거 검증은 v5.1 `8360b6e053472ba20acdfc62b7489e33122997ec`의
14/14 결과입니다. v0.12 최초 18/18 검증은 같은 HEAD에 Network 미커밋 확장을 포함한
working tree에서 수행했습니다. 그 Network 구현은 이후 `0a3f2a6`으로 dev에 반영됐고,
현재 `d41fc2f`에서는 CMake project 및 실제 Network FrameworkVersion이 모두 5.2.0입니다.
NetworkProtocolVersion은 1.0을 유지합니다.
과거 결과를 공개 v5.2 커밋의 재검증 결과로 바꾸지 않으며 후속 검증은 별도로 기록합니다.

Framework를 교체한 뒤에는 **실행 중인 Tool GUI도 모두 종료하고 새 바이너리로
다시 실행**해야 합니다. 같은 경로의 파일을 rebuild해도 이미 실행 중인 프로세스는
이전 Core를 계속 사용하며, GUI Refresh로 실행 파일이 교체되지 않습니다.
`readlink /proc/<GUI_PID>/exe`가 `(deleted)`로 끝나면 rebuild 전 프로세스인지
확인하십시오. 실제 실행 파일은 `sha256sum /proc/<GUI_PID>/exe build-v013/kcf_tool`로
비교할 수 있습니다. 구버전 Tool의 `Protocol error` 사례와 정확한 반환 경로는
[수동 재연결 진단](docs/V51_MANUAL_RECONNECT_DIAGNOSIS.md)에 기록했습니다.

```sh
git -C /home/kssvm/workspace/kcf/kss_control_framework rev-parse HEAD
git -C /home/kssvm/workspace/kcf/kss_control_framework status --short
cmake -S . -B build-v013 \
  -DKCF_SOURCE_DIR=/home/kssvm/workspace/kcf/kss_control_framework \
  -DKCF_TOOL_BUILD_TESTS=ON \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_PREFIX_PATH=/opt/Qt/6.9.3/gcc_64
cmake --build build-v013 -j
./build-v013/kcf_tool --backend kcf
ctest --test-dir build-v013 --output-on-failure
```

Qt가 기본 검색 경로에 있으면 `CMAKE_PREFIX_PATH`는 생략합니다.
`-DKCF_TOOL_BUILD_GUI=OFF`로 Qt 없는 backend 전용 빌드도 가능합니다.
`KCF_SOURCE_DIR`를 생략한 빌드의 `--backend kcf`는 기존 disconnected stub입니다.
실제 통합 테스트는 하드웨어 없이 테스트용 프로세스, SHM 및 loopback UDP를 사용합니다.
자동 통합 테스트는 다른 KCF 프로세스가 없는 환경에서 실행하십시오.
일반 GUI 사용 시에는 탐색할 Application을 먼저 실행합니다.

### v0.11 당시 Framework v5.1 연동 검증 결과

2026-09-19, 위 SHA의 Framework로 새 디렉터리
`/tmp/kcf-tools-v51-8360b6e`에서 Qt GUI와 테스트를 빌드했습니다.
**자동 테스트: CTest 14/14 PASS**. 실제 Backend, codec, registration hardening,
Topic Echo, 지연·반복 재연결, Parameter, Service, Explorer 테스트를 포함합니다.

- Depth=1: 기존 payload·배열 표시, Publish 후 Sequence 증가, 재생성 STALE 및 재연결 PASS.
- Depth=4: Queue 깊이를 초과한 연속 Publish 뒤 최신 payload 읽기, 필드·Sequence 검증,
  재생성 STALE 및 재연결 PASS. Backend와 Qt Echo smoke를 각각 실행했습니다.
- Backend/GUI 구현은 수정하지 않고 기존 `ReadLatest()` 경로를 유지했습니다.
- 자동 GUI 검증은 `QT_QPA_PLATFORM=offscreen` smoke입니다.

**실제 GUI 수동 확인(사용자 확인):** Echo 값 갱신 및 SHM 재생성 후
Refresh → Publisher 재선택 → Start Echo 재연결을 확인했습니다.
이는 자동 테스트 14/14와 별도의 확인 결과이며, 이번 문서 정리에서 재테스트하지 않았습니다.
수동 확인 중의 `Protocol error`는 구버전 GUI 혼용으로 발생했으며
[진단 문서](docs/V51_MANUAL_RECONNECT_DIAGNOSIS.md)에 원인과 비교 재현을 기록했습니다.

실행 로그: `/tmp/kcf-tools-v51-8360b6e/Testing/Temporary/LastTest.log`.
이 결과는 아래 KT-8의 과거 검증 기록과 구분합니다.

## KT-8 검증 프로젝트 실행

KT-8 결과는 당시 Framework 기준의 기록입니다. 아래 명령은 독립 프로젝트가
준비된 환경에서 v5.1로 재빌드하는 방법이며, 이번 v5.1 연동 검증 범위는
Tool 저장소의 Backend·GUI 자동 테스트와 위에 구분한 실제 GUI 수동 확인입니다.

독립 프로젝트는 현재 `/tmp/kcf_generic_validation`에 있습니다. Tool 저장소에
포함된 디렉터리가 아니므로 다른 환경에서는 해당 프로젝트를 별도로 준비하고
아래 경로를 조정해야 합니다. `/tmp`의 소스·빌드·로그는 임시 파일입니다.

### 빌드 및 자동 검증

```sh
cmake -S /tmp/kcf_generic_validation -B /tmp/kt8-v51-build \
  -DKCF_SOURCE_DIR=/home/kssvm/workspace/kcf/kss_control_framework \
  -DKCF_TOOLS_SOURCE_DIR=/home/kssvm/workspace/kcf/kcf_tools \
  -DCMAKE_PREFIX_PATH=/opt/Qt/6.9.3/gcc_64
cmake --build /tmp/kt8-v51-build -j4
ctest --test-dir /tmp/kt8-v51-build --output-on-failure
```

CTest가 두 Application과 Standalone을 실행하고 Qt offscreen에서 실제 GUI를
조작합니다. 로컬 SHM, loopback UDP 및 프로세스 실행 권한이 필요합니다.
고정 endpoint 이름과 discovery 개수를 사용하는 테스트이므로 수동 실행 중인
검증 Application을 모두 종료한 후 실행하고, 기존 Tool 회귀와도 순차 실행합니다.
결과 로그는 `/tmp/kt8-v51-build/Testing/Temporary/LastTest.log`, typed readback·watcher
증거는 테스트가 출력하는 `/tmp/kt8-evidence-*` 디렉터리에 남습니다.

### Application과 GUI 수동 실행

위 빌드를 완료한 후 각 명령을 별도 터미널에서 실행합니다. 실제 Tool GUI는
앞의 `build-v013` 구성으로 빌드하고, 화면 표시가 가능한 데스크톱 세션에서 실행합니다.

```sh
# 터미널 1: Alpha — sensor / processor
/tmp/kt8-v51-build/sensor_suite application /tmp/kt8-manual-alpha /validation
```

```sh
# 터미널 2: Beta — controller / actuator
/tmp/kt8-v51-build/control_suite application /tmp/kt8-manual-beta /validation
```

```sh
# 터미널 3: Standalone
/tmp/kt8-v51-build/diagnostic_node
```

```sh
# 터미널 4: Tool 저장소 디렉터리에서 실행
./build-v013/kcf_tool --backend kcf
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
/tmp/kt8-v51-build/control_suite application /tmp/kt8-manual-beta2 /validation/second
```

종료할 때는 GUI를 닫고 각 Application/Standalone 터미널에서 Ctrl+C를 누릅니다.
남아 있는 GUI에서는 Refresh로 종료된 Runtime을 제거합니다. Endpoint 재생성이나
stale 오류 후에도 Refresh하고 새 endpoint를 선택해 접근합니다.

당시 검증 결과: **KT-8 1/1, 기존 Tool 10/10, KCF-less GUI 3/3·backend 1/1,
Framework 회귀 모두 PASS**. Tool production 코드와 KCF core 변경 없이 검증했습니다.
세부 항목과 실행 범위는 [KT-8 결과 보고서](docs/KT8_GENERIC_CROSS_APPLICATION_VALIDATION.md)를 참고하십시오.

## Graph 구조 시각화

Graph 탭은 현재 discovery 스냅샷으로 Application별 Element와 Topic 관계를 표시합니다.
외부 Graphviz 없이 Qt QGraphicsView/QGraphicsScene을 사용합니다.

- 왼쪽: Application별 사각형 Element. 같은 Application 이름도 Supervisor PID/start_ticks로 구분하며 Standalone은 별도 그룹입니다.
- 오른쪽: 작은 Topic/Parameter endpoint와 개별 Service server 노드. Publisher → Topic → Subscriber 방향으로 연결합니다.
- 관계의 Element는 `EndpointIdentity.runtime`으로 찾습니다. 표시용 publisher/subscriber 이름 문자열로 추정하지 않습니다.
- Topics/Parameters/Services toggle과 Application filter(All 포함)로 표시를 조절합니다. 필터는 선택한 Application에 속한 Element와 그 endpoint만 남깁니다.
- Parameters는 discovery된 OWNER/CLIENT만, Services는 SERVER 등록만 연결합니다. Service client는 추정하지 않습니다.
- Refresh Graph는 기존 Refresh 경로를 사용합니다. Echo가 중지되며 미저장 Parameter 편집은 Apply/Revert 후 새로고침할 수 있습니다.
- 휠 또는 +/−로 확대·축소, 빈 배경 드래그로 이동, Fit으로 전체 배치를 봅니다.
- Element를 더블클릭하거나 선택 후 Enter를 누르면 Elements로 이동합니다. Topic에 여러 endpoint가 있으면 역할·PID/start_ticks·registration 목록에서 선택하여 Topics의 정확한 항목으로 이동합니다.

같은 이름의 Topic/Parameter는 하나의 논리 채널 노드로 묶되 각 discovery registration의
관계는 유지합니다. 고정된 두 열에 identity/이름 순서로 배치하며 force-directed 배치는
사용하지 않습니다. 실제 통신 성공 여부, 데이터 전달량, 미발견 client 관계는 추정하지 않습니다.
Remote metadata와 실제 Diagnostics Route 관측은 아래 v0.13 Network 확장으로 제공합니다. [Graph 구현 및 검증](docs/GRAPH.md)을 참고하십시오.

## v0.13 Network 관측

KCF v5.3 공개 Diagnostics API가 있는 위 working tree로 빌드하면, 기존 Discovery와 함께
ONLINE·호환 가능·NETWORK_DIAGNOSTICS 지원 Gateway의 실제 Snapshot을 조회합니다.
구형 Network Framework에서는 기존 Remote Discovery-only 기능을 유지합니다.

```sh
./build-v013/kcf_tool --backend kcf --network-interface 192.168.1.10
# loopback 테스트: --network-interface 127.0.0.1 --network-port 37651
# Local 전용: --backend kcf --no-network
```

- Graph의 Remote Host → Application → Element → T/P/S/A metadata와 읽기 전용
  상세에서 ONLINE/LOST, 버전, compatibility reason, capabilities, type 정보를 봅니다.
- 수동 Refresh로 worker snapshot을 표시합니다. Host/Application 필터,
  Local/Remote·T/P/S/A toggle, 기존 Zoom/Pan/Fit을 제공합니다.
- Remote endpoint는 Host + boot + PID/start_ticks + registration으로 구분합니다.
  최신 Remote Client API 구성에서는 Graph 선택을 해당 Host의 Remote 기능 화면으로 연결합니다. Local 기능은 기존과 같습니다.
- Remote Topic Detail에서 Gateway별 Route·Counter·Rate를 확인합니다. Proxy Publish Hz는
  Application Subscriber 처리율이 아닙니다. 미측정/오류는 Measuring/Unavailable로 구분합니다.
- Graph는 양쪽 Gateway의 전체 identity/session/route_id·송수신 역할과 ACTIVE 상태,
  10초 이내의 성공 관측 및 실제 registration을 검증한 Route만 청록색 방향 실선으로 표시합니다.
  metadata ownership은 점선이며, 이름/type만으로 cross-host 연결을 추정하지 않습니다.
- Graph는 수동 Refresh입니다. 표시 후 자동 만료되지 않으며, 다음 Refresh/재그리기에서
  최신성 조건을 다시 검사합니다.
- Tool은 forwarding/Gateway가 아닙니다. 최신 Framework Remote Client API로 빌드하면
  [Remote Control / Multi-Host UI](docs/REMOTE_CONTROL.md)의 Get/Set/Call/Goal/Cancel 및 Topic 구독을 제공합니다.
  구형 Framework의 관측 전용 호환성은 유지합니다.

빌드 요구사항, 현재 제한과 이번 검증은 [Network 관측](docs/NETWORK_OBSERVATION.md)을
참고하십시오. 실제 물리 LAN 검증과 Framework Virtual LAN의 기존 PASS는 별도입니다.

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
- Echo의 Frequency는 Local Topic transport sequence 증가량을 `steady_clock` 경과 시간으로 나눈 관측 Publish Hz입니다. 최근 약 1초(최대 16개)를 보관하고 0.5초 이상 모이기 전에는 `Measuring...`를 표시합니다.
- 100 ms UI polling과 Publish Hz는 별개입니다. Local Echo는 payload timestamp 기반 source rate나 Remote route rate를 측정하지 않습니다. Remote Network rate는 별도 Diagnostics 관측입니다.
- Stop·오류·Topic 변경·Refresh에서는 측정 기록을 지우고 기존 정적 Frequency 또는 `N/A`를 복원합니다. sequence가 감소하면 측정을 다시 시작합니다.
- Echo 동안 하나의 reader를 유지하며, R4.1의 SHM 재생성 감지를 사용합니다.
- Stop Echo, Topic 변경, Refresh, 창 닫기 또는 치명적 읽기 오류 시 polling을 중지합니다.
- 재생성 감지 후 Refresh와 Start Echo가 필요합니다. 첫 sample 대기는 자동 중지하지 않습니다.
- Dynamic Monitor API는 계속 `-ENOTSUP`입니다. Launch 확장은 미구현입니다.
- Mock의 시연 데이터와 기존 독립 테스트는 유지됩니다.
- 실제 Open 전후와 Parameter Set / Service Call 직전에 cached registration identity와 현재 registry를 비교합니다.
  사라지거나 바뀐 registration은 `-ESTALE`이며 Refresh 후 새 endpoint를 선택해야 합니다.
- 정상 시작한 Echo의 polling에는 registry 조회를 추가하지 않습니다. 이후 SHM 재생성은 기존 R4.1이 감지합니다.
- Parameter/Service float 입력은 finite 값만 허용합니다. NaN/Inf와 overflow 입력은 거부하며, 읽은 NaN/Inf는 관측용으로 표시할 수 있습니다.
- 실제 backend가 측정하지 않는 heartbeat/frequency와 sample 이전 sequence는 `N/A`로 표시합니다.
- registration 검증은 원자적 보장이 아닙니다. 특히 Service protocol에는 registration_id가 없으므로
  마지막 검증 직후 재등록되는 극단적인 race까지 막지는 못합니다. 발견한 stale metadata로는 Call하지 않습니다.

Framework v5.1 호환성: **Topic format = 4, Service protocol = 2**.
Depth=1 및 Depth=4 Topic 모두 `DynamicTopicReader::ReadLatest()`로 최신 payload만
읽습니다. Tool은 `ReadNext()`나 Subscriber cursor를 사용하지 않으며 Queue 이력을 소비하지 않습니다.
Topic format 3으로 생성된 기존 storage/바이너리를 format 4와 혼용하지 않습니다.
Supervisor와 child는 동일 Framework v5.1 revision으로 **rebuild + restart**해야 합니다.
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
