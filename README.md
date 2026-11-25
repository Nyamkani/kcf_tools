# KCF Tools — v0.16.0

Linux C++17 / Qt6 Widgets 기반 KCF GUI 도구입니다.

## 최신 Release

- **Tools:** v0.16.0
- **Release 빌드 기준:** KCF Core v5.6.2
- **실행 파일:** `kcf_tools`
- **패키지 기본 Backend:** `kcf` (Real)
- **Mock 실행:** `kcf_tools --backend mock`
- **지원 패키지:** Ubuntu 24.04 / 26.04, AMD64 / ARM64

이번 버전에서는 Local/Remote Topic Plot, CSV Export,
Remote Workspace 개선, 실행 파일 명칭 및 CI/CD 배포 구성을
정리했습니다.

자세한 변경 사항과 검증 한계는
[Release Notes](docs/releases/v0.16.0.md)를 참고하세요.

---

## 이전 버전 기록

# KCF Tool — v0.15.0

Linux C++17 / Qt6 Widgets 기반 standalone Tool입니다. GUI는 `ToolBackend`만 사용하며,
기본 `MockBackend`와 선택적인 실제 `KcfBackend`를 제공합니다.

KCF 5.6.1 실제 Backend를 포함한 DEB 생성·검증은 [Debian 패키징 가이드](docs/DEBIAN_PACKAGING.md)를 참고하십시오.

## v0.15.0 기준

현재 버전은 **v0.15.0** (CMake **0.15.0**)입니다. 기존 Local 기능과 함께 다음 Remote 기능을 제공합니다.

- Multi-Host Discovery / Host 선택 및 metadata 기반 Remote Application / Element / Endpoint 탐색
- Network Diagnostics: Gateway별 Route identity/session·상태·9개 Counter와 Send/Receive/Proxy Publish Hz, Skipped/s
- 양쪽 Gateway에서 확인한 ACTIVE Route만 연결하는 Verified Route Graph와 Remote 상세
- Remote Parameter Get / Set / Readback
- Remote Service Call / Response
- Remote Action Goal / Feedback / Result / Cancel (ACK와 Terminal State 구분)
- Remote Topic Subscribe / Unsubscribe, 실제 Payload·Sequence 및 수신률 표시
- TypeDescriptor 기반 generic scalar/fixed-array 입력·출력과 Framework canonical codec 사용

Local 화면은 기존 Runtime 탐색, Topic Echo, Parameter Editor, Service Call을 유지합니다.
Remote Host를 선택하면 해당 Host의 탐색·조작 화면을 사용하며, Local Runtime 연결과
Network 상태를 별도로 표시합니다. SystemStatus는 Local 전용입니다.

실제 Backend는 **Core v5.6.1 이상과 필수 공개 API**를 요구합니다. Configure에서
`FrameworkVersion`, `PeerInfo.last_complete_snapshot`의 valid/stale/revision/endpoints 타입,
`RemoteControlClient`, `RemoteTopicClient`, `NetworkDiagnosticsClient`를 컴파일 검사합니다.
Core 5.6.0 및 필수 API가 없는 소스는 명확한 오류로 거부하며 구형 API로 우회하지 않습니다.
Core를 지정하지 않은 Mock/stub 빌드는 유지합니다. Debian 패키징은 이번 릴리스 기준 **정확히 Core 5.6.1**을 요구합니다.
Tool은 공개 API를 사용하며 Gateway/forwarder나 Application의 장치 제어 로직을 대신하지 않습니다.

v0.15에서는 [Ownership / Communication Graph](docs/GRAPH.md#v015-communication-graph)를 제공합니다.
Local Topic 등록 관계는 Metadata 기반 점선, Remote의 양쪽 검증 ACTIVE Route는 청록색 실선으로 구분하며
어느 쪽도 Application 수신 성공을 보장하지 않습니다. Remote Workspace는 Metadata 갱신 중
`last_complete_snapshot`을 **UPDATING / STALE 표시 전용**으로 유지하고 신규 제어를 차단합니다.
[READY / STALE / INVALID 계약](docs/REMOTE_CONTROL.md#v015-metadata-states)을 참고하세요.

### v0.15 Release 준비 검증

최종 Debug 검증은 **Core v5.6.1 / Qt 6.9.3** 개발 환경에서 `build-main`을 재구성·증분 빌드했습니다.
PeerDiscovery 재컴파일과 정적 Core/Tool 재링크를 확인했고 **CTest 22/22 PASS (92.44초)**,
별도 **Mock/stub 6/6 PASS (4.08초)**입니다. **Virtual LAN / Qt offscreen E2E도 PASS**로,
READY/STALE/INVALID, 선택·Subscribe Handle 유지, LOST/Session 변경, Verified Route 및
Ownership/Communication Graph를 검증했습니다. 최초 sandbox netlink 제한에 따른 환경 준비 실패와
격리 namespace 재실행 PASS를 구분합니다.

**Ubuntu 24.04 amd64 / Release / 배포판 Qt 6.4.2**에서 Core v5.6.1 DEB 추출 SDK로
Configure / Build / CPack 및 추출 실행 검증을 완료했습니다.
`kcf-tools_0.15.0-1+ubuntu24.04_amd64.deb`는 **784,686 bytes**이며 SHA256은
`1b76bd756972996b3f8901349c883c70fb399f88c584697390a1879311e9bc7b`입니다.
[패키징 결과·SDK staging 재현 방법](docs/DEBIAN_PACKAGING.md)을 참고하세요.
Tag/GitHub Release는 생성하지 않았습니다.

실제 DEB 설치·업그레이드, 이번 Release의 실제 데스크톱 GUI·물리 LAN·ARM64 및
Release 빌드의 전체 CTest는 **미검증**입니다. 추출 offscreen startup을 설치 검증으로 간주하지 않습니다.
과거 Local Communication GUI 사용자 확인 PASS는 별도 이력이며 다중 관계 GUI·실물 Remote Graph는 미검증입니다.
Historical: 앞선 버전 정합화의 관련 Backend·Network **7/7 PASS (57.17초)**,
Mock/stub **6/6 PASS (3.84초)** 및 Core 5.6.0 Configure 거부 PASS를 보존합니다.
기존 [Virtual LAN E2E 기록](docs/NETWORK_OBSERVATION.md#v015-snapshot-validation)과 이번 최종 회귀는 별도 실행입니다.
이번 문서 작업에서는 빌드·회귀·패키징을 재실행하지 않았습니다.

Workspace / Manifest / Launch Designer는 **Tools #3 후속 과제**이며 현재 구현 기능이 아닙니다.

Historical: 2026-10-21 **Framework v5.3 / Tools v0.13 소스의 사용자 확인 물리 LAN 수동 검증**에서 Orange Pi 5+ ↔ Ubuntu VM Bridged LAN의
실제 Qt GUI로 Discovery/Metadata, Topic 수신(약 **45.96Hz 관측**), Parameter Get/Set/Readback,
Service Call/Response 및 Action 성공·Cancel을 확인했습니다.
이는 v0.14/v0.15 재검증이나 전체 안정성 인증이 아니며, 개별 프로그램 재시작·LAN 단절 복구와 실제 다중 Gateway 동시 제어는
추가 검증이 필요합니다. [물리 LAN 검증 및 제한](docs/REMOTE_CONTROL.md#물리-lan-수동-검증--사용자-확인)을 참고하십시오.

[Remote Control 사용·권한 설정](docs/REMOTE_CONTROL.md),
[Network 계약](docs/NETWORK_OBSERVATION.md#v013-network-diagnostics-계약),
[Verified Route Graph](docs/GRAPH.md#v013-verified-network-route-graph)를 참고하십시오.
Historical: v0.14의 실제 빌드·검증 범위와 Framework 커밋 기준은 [v0.14 릴리스 노트](docs/V0_14_RELEASE_NOTES.md)를 따릅니다.

## Historical — NET56 개발 연동 (Tools 0.14.0)

현재 Framework **5.5.0 개발 작업 트리(NET56-4C/5B1 포함)**와 연동하면 Host ID 없이
`kcf_tool --backend kcf`로 임시 Topic Observer를 시작할 수 있습니다. 공개 REMOTE Publisher를
선택하여 Descriptor 조회·Subscribe/Unsubscribe/Poll을 수행합니다. 자동 재구독은 없습니다.
임시 Observer의 Capability는 DISCOVERY / REMOTE_TOPIC_RECEIVER / TARGET_ENDPOINT이며
Parameter Get/Set, Service, Action 제어 버튼은 활성화하지 않습니다. 명시적 `--network-host-id`
사용 시 기존 제어 기능과 서버의 독립 Allowlist 검사는 유지합니다. Gateway 영구 ID를 복사하지 마세요.

같은 UID의 Local Gateway는 기본 `/run/kcf/gateway.sock`에서 조회합니다.
`--gateway-socket PATH`로 개발 경로를 지정하며 상태/Peer Snapshot은 Direct Discovery와 별도로 표시합니다.
Local IPC 실패가 Direct Observer를 종료시키지 않으며, IPC 연결은 Payload/Control 권한이 아닙니다.

```sh
./build/kcf_tool --backend kcf
# 개발 Gateway가 지정한 실제 socket 경로 사용
./build/kcf_tool --backend kcf --gateway-socket /path/to/private-runtime/gateway.sock
```

Gateway는 노출 Prefix 정책을 활성화해야 합니다. `--network-interface`는 **IPv4 주소**이며
Framework Gateway의 `--interface` NIC 이름 옵션과 다릅니다. Discovery 기본 UDP 포트는 37651입니다.
구형 Framework에서는 기능 탐지 결과에 따라 Discovery-only/지원 불가 상태를 표시합니다.

구현·검증·미검증의 구분 및 기존 이력은 [Network Observation](docs/NETWORK_OBSERVATION.md)을 참고하세요.
NET56 개발 구현은 Framework v5.6.0 또는 새로운 Tools 릴리스 배포 완료를 뜻하지 않습니다.

## Historical — Framework v5.5 호환성 검증 — 2026-10-22

Tools 버전은 **v0.14 / 0.14.0**을 유지합니다. Framework dev
`f184787d7d68e8e1d3adbcd27d9ee09d170893a9` (**5.5.0**)의 공개 API로 검증했습니다.
`KCF_SOURCE_DIR=/home/kssvm/workspace/kcf/kss_control_framework`이며, Framework의 기존 미커밋 문서 변경은
그대로 두고 소스는 변경하지 않았습니다. 기존 v5.4 검증은 [v0.14 릴리스 노트](docs/V0_14_RELEASE_NOTES.md)에 보존합니다.

- 새 `/tmp/kt-v55` Debug configure/build 및 Qt 6.9.3 빌드: 최초 PASS.
- 전체 CTest: **19/19 PASS, 50.28초**, 최초 실행. 격리된 Loopback/SHM에서 Local Application/Element 탐색,
  Topic Echo·재연결(depth 1/4), Parameter, Service, Graph, Network Discovery/Diagnostics와 Qt offscreen 회귀를 실행했습니다.
- 별도 `/tmp/kt-v55-mock`의 KCF 없는 Debug configure/build 및 CTest: 최초 **6/6 PASS, 2.75초**.
- 기존 `tests/remote_control_e2e.py`의 **Virtual LAN / Qt offscreen E2E: 최초 PASS**.
  Remote Parameter Readback, Service, Action, Topic, Session 변경, GUI 재설정, 진행 중 Stop을 확인했습니다.
  의도적 Timeout은 `execution_unknown=1`, `response_received=0`, Service 실행 1회로 확인했습니다.
- 이번 검증에는 실패 후 재실행이 없었습니다. 로그는 `/tmp/kt-v55-tests.log`, `/tmp/kt-v55-mock-tests.log`,
  `/tmp/kt-v55-remote-e2e/results.json`에 있으며 임시 개발 파일입니다.

v5.5 `ApplicationState::STOPPED`를 표시하도록 변환을 추가했습니다. 구형 Framework 헤더에는
STOPPED가 없어도 빌드 가능하도록 처리하며, 기존 상태 변환과 enum 숫자값은 변경하지 않습니다.
Supervisor 종료 후 metadata가 제거될 수 있으므로 STOPPED의 영구 관측은 요구하지 않습니다.
real fixture는 Setup 성공 후 Run에 진입하고 테스트가 실제 RUNNING/ERROR를 확인하므로,
`Setup()==0`이 RUNNING 보장은 아닌 v5.5 계약과 호환됩니다.

물리 LAN·실제 데스크톱 GUI 수동 시험과 구형 Framework 전체 재빌드는 이번에 수행하지 않았습니다.
RemoteTopicClient의 기존 간헐적 Timeout은 **Known Issue**로 유지하며 이번 PASS로 해결되었다고 판단하지 않습니다.
Timeout 증가·임의 sleep·자동 retry·Framework 수정은 하지 않았습니다.

## Historical — v0.13 Remote Control 개발 및 검증

v0.13에서 Generic Remote Control / Multi-Host UI를 구현했고 Framework v5.3과 물리 LAN 수동 검증을 진행했습니다.
[v0.13 릴리스 노트](docs/V0_13_RELEASE_NOTES.md)의 당시 커밋 기준점·PASS/FAIL·재실행 이력은 보존합니다.
그 결과를 v0.14 / Framework v5.4의 재검증 결과로 소급하지 않습니다.

## Historical — 초기 v0.13 Diagnostics-only 단계

아래의 “현재/이번”과 미커밋 Framework 기준은 KT-ND3~KT-ND5B 당시 기록입니다.
기능 범위·검증 수치·실패 및 재실행 이력을 보존하며, 이후 Remote Control 구현과 사용자 물리 LAN 검증은 위 설명과 구분합니다.
[v0.13 릴리스 노트](docs/V0_13_RELEASE_NOTES.md)는 현재 v0.13 기능과 후속 물리 LAN 검증을 포함합니다.
초기 Diagnostics 구현·검증 이력은 릴리스 노트 내부의 Historical 기록으로 구분합니다.

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
[Changelog](Changelog.md)를 참고하십시오.

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

KCF 연결 빌드(`KCF_SOURCE_DIR` 설정)는 **nlohmann_json >= 3.2**의 CMake 패키지가 필요합니다
(`nlohmann_json::nlohmann_json`, Debian/Ubuntu 패키지 이름 `nlohmann-json3-dev`).
Local Gateway IPC JSON 검증에 사용하며 GUI OFF인 KCF Backend 빌드에도 필요합니다.
Framework 없는 Mock/stub 구성에는 이 조건부 의존성을 요구하지 않습니다. 이 문서 작업은 패키지를 설치하지 않습니다.

저장소 루트에서 **`build-main`을 표준 개발 빌드 디렉터리**로 사용합니다.
현재 Tools v0.15.0과 형제 저장소의 Core v5.6.1 소스를 연결하고 Debug, Qt GUI,
테스트를 활성화합니다. 다음은 개발 VM에서 확인한 Qt **6.9.3** 구성입니다.

```sh
cmake -S . -B build-main \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_SHARED_LIBS=OFF \
  -DKCF_SOURCE_DIR=../kss_control_framework \
  -DKCF_TOOL_BUILD_GUI=ON \
  -DKCF_TOOL_BUILD_TESTS=ON \
  -DKCF_TOOL_ENABLE_PACKAGING=OFF \
  -DCMAKE_PREFIX_PATH=/opt/Qt/6.9.3/gcc_64
cmake --build build-main -j2
# GUI를 시작하지 않는 CLI 확인
QT_QPA_PLATFORM=offscreen ./build-main/kcf_tool --help
# 아래 GUI 명령은 데스크톱 세션에서 각각 실행합니다.
./build-main/kcf_tool --backend mock
./build-main/kcf_tool --backend kcf
# Local Application만 탐색하고 Network Discovery를 끄려면:
./build-main/kcf_tool --backend kcf --no-network
```

실행 파일은 `build-main/kcf_tool`입니다. 실제 Core 연결 빌드에서도 Mock Backend를
선택할 수 있으며, 옵션을 생략하면 기본 Backend는 Mock입니다. 실제 Local Backend는
탐색할 Application과 동일한 UID로 실행합니다. Remote 연결의 interface 주소는
해당 PC에 실제 존재하는 주소로 지정합니다.

Core 없는 Mock/stub 전용 빌드가 필요한 경우에만 별도 `build-mock`을 사용합니다.
표준 `build-main`의 실제 Backend 설정을 Mock 설정으로 덮어쓰지 않습니다.

```sh
cmake -S . -B build-mock \
  -DCMAKE_BUILD_TYPE=Debug -DKCF_SOURCE_DIR= \
  -DKCF_TOOL_BUILD_GUI=ON -DKCF_TOOL_BUILD_TESTS=ON \
  -DKCF_TOOL_ENABLE_PACKAGING=OFF \
  -DCMAKE_PREFIX_PATH=/opt/Qt/6.9.3/gcc_64
cmake --build build-mock -j2
./build-mock/kcf_tool --backend mock
```

현재 실제 Backend는 **Core 5.6.1 이상 및 필수 API가 포함된 소스**를
`KCF_SOURCE_DIR`로 참조합니다. 소스를 복사하지 않으며 HEAD만으로 미커밋 변경을
식별할 수 없으므로 `git status`와 적용된 API도 확인합니다. 새 빌드 디렉터리를 사용합니다.
초기 Diagnostics-only 빌드의 F-ND5B 기준은 위 Historical 기록으로 보존합니다.
Historical: v0.14 최초 Framework 검증 기준은 dev `5ae507a8b866015360686463c1ce03905c4e39af` (v5.4.0)이며,
실행 결과는 [릴리스 노트](docs/V0_14_RELEASE_NOTES.md)에 기록합니다.

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
확인하십시오. 실제 실행 파일은 `sha256sum /proc/<GUI_PID>/exe build-main/kcf_tool`로
비교할 수 있습니다. 구버전 Tool의 `Protocol error` 사례와 정확한 반환 경로는
[수동 재연결 진단](docs/V51_MANUAL_RECONNECT_DIAGNOSIS.md)에 기록했습니다.

전체 회귀는 운영 프로세스와 분리된 테스트 환경에서 별도로 실행합니다.

```sh
ctest --test-dir build-main --output-on-failure
```

Qt가 기본 검색 경로에 있으면 `CMAKE_PREFIX_PATH`는 생략합니다.
`-DKCF_TOOL_BUILD_GUI=OFF`로 Qt 없는 backend 전용 빌드도 가능합니다.
`KCF_SOURCE_DIR`를 생략한 빌드의 `--backend kcf`는 기존 disconnected stub입니다.
실제 통합 테스트는 하드웨어 없이 테스트용 프로세스, SHM 및 loopback UDP를 사용합니다.
자동 통합 테스트는 다른 KCF 프로세스가 없는 환경에서 실행하십시오.
일반 GUI 사용 시에는 탐색할 Application을 먼저 실행합니다.

### Debian 패키지 설치 후 실행과 Gateway 운영

위 소스 빌드는 시스템 설치 없이 `build-main/kcf_tool`을 실행합니다.
관리자가 호환되는 Tools DEB를 설치한 환경에서는 다음 설치 바이너리를 사용합니다.

```sh
/usr/bin/kcf_tool --help
/usr/bin/kcf_tool --backend mock
/usr/bin/kcf_tool --backend kcf
```

DEB 생성·설치·Qt 런타임 의존성은 [패키징 가이드](docs/DEBIAN_PACKAGING.md)를 따릅니다.
개발용 `/opt/Qt/6.9.3` 설정과 배포판 Qt 기반 DEB 구성은 별도입니다.
Core는 Tools 바이너리에 정적 링크되므로 Core 패키지만 교체해도 Tools가 갱신되지는 않습니다.

Tools는 Gateway를 생성하거나 종료하지 않습니다. Local Gateway IPC 기본 경로는
`/run/kcf/gateway.sock`이며 `--gateway-socket`으로 지정할 수 있습니다.
Gateway를 운영하는 Host에서는 Application과 같은 UID의 공통 systemd 서비스를 이용하며,
이미 실행 중인 서비스와 수동 Gateway를 중복 실행하지 않습니다. 별도 PC의 Tools에서
원격 Host를 탐색하기 위해 로컬 Gateway를 반드시 실행할 필요는 없습니다.
Gateway의 수동 실행·최초 계정 설정·자동 시작 승인 및 Topic/Control 허용 정책은
Core 저장소의 `gateway/systemd/README.md`를 따릅니다. GUI 종료는 Gateway 종료가 아닙니다.

과거 검증 로그와 `/tmp` 빌드 경로는 당시 기록으로 보존하며,
현재 개발 GUI의 권장 실행 경로는 `build-main/kcf_tool`입니다.

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
앞의 `build-main` 구성으로 빌드하고, 화면 표시가 가능한 데스크톱 세션에서 실행합니다.

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
./build-main/kcf_tool --backend kcf
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
기본 Ownership과 Communication 모드를 선택할 수 있습니다. 아래 배치는 Ownership 기준이며,
Communication은 Element 간 Topic 방향·라벨과 Local Metadata 점선/Remote Verified 실선을 표시합니다.

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
Remote metadata와 실제 Diagnostics Route 관측은 아래 v0.15 Network 기능으로 제공합니다. [Graph 구현 및 검증](docs/GRAPH.md)을 참고하십시오.

## v0.15 Network 관측

Core >=5.6.1 필수 API 구성에서는 기존 Discovery와 함께
ONLINE·호환 가능·NETWORK_DIAGNOSTICS 지원 Gateway의 실제 Snapshot을 조회합니다.
Diagnostics 미지원 Gateway는 미지원 상태로 구분하며 실제 Route를 추정하지 않습니다.

```sh
./build-main/kcf_tool --backend kcf --network-interface 192.168.1.10
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
  구형 Core 소스의 관측 전용 fallback은 v0.15 Configure에서 허용하지 않습니다.

Diagnostics 계약과 과거 자동 검증은 [Network 관측](docs/NETWORK_OBSERVATION.md),
현재 Remote 조작과 사용자 물리 LAN 검증은 [Remote Control](docs/REMOTE_CONTROL.md)을 참고하십시오.
물리 LAN 수동 결과와 Virtual LAN/Qt offscreen 자동 결과는 별도입니다.

## 동작

아래는 기존 Local 기능의 동작입니다. Remote 조작은 [Remote Control](docs/REMOTE_CONTROL.md)을 따릅니다.

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
