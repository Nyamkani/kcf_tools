# Generic Remote Control / Multi-Host UI

KCF Tools **v0.14** (CMake **0.14.0**)의 현재 Remote 기능을 설명합니다.
Framework 연동 기준은 **KCF v5.4**이며, 실제 검증 범위와 Framework 커밋 기준은
[v0.14 릴리스 노트](V0_14_RELEASE_NOTES.md)에 구분합니다. 공개 Release/tag는 생성하지 않습니다.
Framework의 `RemoteControlClient`, `RemoteTopicClient`, TypeDescriptor Query가 있는
최신 sibling working tree를 사용합니다. 구형 Framework는 기존 관측 기능을 유지하고,
Framework 없는 Mock/stub에서는 Remote 조작이 지원되지 않습니다.

## 빌드 및 실행

Qt6 Widgets와 Network 개발 모듈이 필요합니다.

```sh
cmake -S . -B build-remote -DCMAKE_BUILD_TYPE=Debug \
  -DKCF_SOURCE_DIR=../kss_control_framework -DKCF_TOOL_BUILD_TESTS=ON
cmake --build build-remote -j4
./build-remote/kcf_tool --backend kcf \
  --network-interface 192.168.1.10 --network-port 37651 \
  --network-host-id 63000000000000000000000000000000
```

IP는 Tool 장비의 실제 IPv4로 변경합니다. Host ID는 장비마다 고유한 32자리
0이 아닌 hexadecimal 값이어야 합니다. 예제 ID를 여러 Tool에 재사용하지 않습니다.
`--no-network` 및 기존 interface/port CLI를 유지합니다.

상단에서 IPv4 interface, Discovery port, Host ID를 지정하고 **Apply network settings**를
누릅니다. Host ID는 Qt settings에 저장되어 다음 GUI 실행의 입력값으로 제공됩니다.
CLI 없이 저장값만 존재하는 경우 자동으로 제어를 활성화하지 않으며 Apply가 필요합니다.
Host ID가 비어 있으면 임시 identity의 관측 전용 Observer입니다.

Gateway에는 같은 Tool Host ID를 필요한 권한별로 설정해야 합니다:
`descriptor_query_clients`, `parameter_get_clients`, `parameter_set_clients`,
`service_call_clients`, `action_goal_clients`, `action_cancel_clients`.
IP로 권한을 추정하지 않습니다. Gateway의 remote scope/type 등록과 Topic export 설정도
필요합니다. Diagnostics/검증된 Route Graph를 사용하려면 양쪽 Gateway의
`diagnostics_enabled`도 활성화합니다. 현재 Tool Topic client는 Gateway Topic UDP 기본 포트 `37652`를 사용하고,
Tool 수신 포트는 OS가 할당합니다. Discovery UDP, Gateway Control TCP, Topic UDP가 통신 가능한 신뢰된 LAN에서
사용합니다. Host ID allowlist는 암호학적 인증이 아니며 TLS 인증을 대신하지 않습니다.

## 사용

- 상단 Host selector의 Local은 기존 Local 화면입니다. Remote Host를 선택하면
  Applications/Elements와 기능 탭이 해당 Host metadata를 표시합니다.
  Local runtime 연결과 Network 상태는 별도로 표시합니다.
  기존 SystemStatus 탭은 Local 전용입니다.
- Parameter: endpoint 선택 → Get → 필드 편집 → **Set + Readback**.
- Service: request 필드 로드/편집 → **Call once** → response 확인.
- Action: goal 필드 편집 → **Start Goal** → 실행 목록에서 Feedback/Result 확인.
  **Cancel selected Goal**의 ACK와 실제 terminal 상태는 별개입니다.
- Topic: 정확한 Publisher endpoint 선택 → **Subscribe** → 최신 값, Sequence, 수신률 확인
  → **Unsubscribe**. 동일 이름도 Host/boot/runtime/registration/session으로 구분합니다.
- Graph의 Remote 항목을 activate하면 해당 Host/기능으로 이동합니다.
  **Show all kinds**로 종류 필터를 모두 켤 수 있습니다.

packed TypeDescriptor와 Framework canonical codec으로 scalar/fixed array를 처리합니다.
숫자 범위, 변환 오류, 배열 크기와 전체 TypeIdentity를 검증합니다.
문자열·가변 배열·enum 의미·단위는 임의로 추가하지 않습니다.
Discovery가 제공하지 않는 endpoint 없는 Remote runtime이나 membership은 추정하지 않습니다.
현재 `PeerInfo`에는 Host 표시 이름이 없으므로 Host label은 실제 Host ID로 표시합니다.

## 수명과 결과 의미

Observer 하나의 PeerDiscovery를 Diagnostics, Control, Topic client가 공유합니다.
동기식 Control/Descriptor 작업은 bounded queue(32)의 전용 worker에서 실행합니다.
Action tracking, Topic 수신 및 기존 Diagnostics worker는 독립적으로 진행됩니다.
GUI는 future 완료 여부만 확인하며 네트워크 응답을 기다리지 않습니다.
Framework의 동시 Action 4개 및 Topic subscription 제한을 따릅니다.

네트워크 설정 변경/종료는 worker에서 Control I/O 중단, 대기 작업 취소,
Topic 정리, Discovery 종료 순서로 처리합니다. 자동 Action Cancel을 보내지 않습니다.
Session/boot/registration 변경이나 LOST 시 선택/편집/표시값을 무효화합니다.
기존 작업이 실제 하드웨어에서 정지했다고 판단하지 않습니다.

`response_received`, `execution_unknown`, `cancel_unknown`을 개별 표시하며,
Set/Call/Goal timeout 후 자동 재시도하지 않습니다. Set 성공과 readback 실패도 구분합니다.
권한 없는 Gateway는 명시적 오류 또는 HELLO 연결 종료로 나타날 수 있습니다.
구형/미지원 구성은 조작 불가 상태를 표시합니다.

Graph는 기존 양쪽 Gateway의 ACTIVE Route 검증과 10초 freshness를 유지합니다.
수동 Refresh 후의 표시를 자동 만료시키지 않습니다. Tool 자체에는 Diagnostics TCP
listener가 없으므로 Tool 수신 subscription을 양쪽 Gateway에서 검증한 Edge로 추정하지 않습니다.
Topic 수신률은 해당 subscription의 `packets_received` 증가량과 worker 관측 시각의
steady-clock 경과 시간(최소 0.5초)으로 계산하며 Application 처리율이 아닙니다.

## 검증 실행

기존 CTest와 별도로 Linux user/net/mount/PID namespace 권한, `ip`, Python3, Qt offscreen을
사용하는 실제 두 Gateway + 독립 Tool 프로세스 통합 테스트가 있습니다.

```sh
QT_QPA_PLATFORM=offscreen unshare --user --map-root-user --net --ipc --mount \
  --pid --fork --kill-child=KILL --mount-proc \
  python3 tests/remote_control_e2e.py ../kss_control_framework \
  build-remote /tmp/kcf-remote-e2e
```

출력 디렉터리의 JSON 및 stderr 로그를 확인합니다. 이 검증은 Virtual LAN/Qt offscreen이며
물리 LAN 및 실제 데스크톱 GUI 수동 검증을 대신하지 않습니다.

<a id="이번-작업의-검증-결과"></a>

## Historical — Remote 통합 당시 자동 검증 결과

다음은 2026-10-21 Remote 통합 당시의 기존 자동 검증 기록입니다. 세 구성 모두 Debug 빌드였으며,
이번 문서 최신화에서 빌드·테스트를 재실행한 결과가 아닙니다.

| 구성 | 전체 CTest |
| --- | --- |
| 현재 sibling Framework working tree | 19/19 PASS, 53.98초 |
| 기존 v5.2 Discovery-only Framework | 18/18 PASS, 32.45초 |
| Framework 없는 Mock/stub | 6/6 PASS, 2.94초 |

Virtual LAN/Qt offscreen에서 실제 Parameter Set/Readback, Service Response,
Action Feedback/Result/Cancel 및 Action 중 다른 요청, 두 Host의 동일 이름 Topic 구독,
GUI Parameter/Service/Action/Topic 조작, 수신 Counter 기반 Hz, Session 변경 거부,
GUI Network 재설정, 진행 중 요청 Stop을 확인했습니다.
마지막 Timeout 검증은 `ETIMEDOUT`, `execution_unknown=1`, `response_received=0`이었고,
fixture의 Service 호출 증가는 정확히 1회였습니다. 자동 재시도로 중복 실행하지 않았습니다.
GUI 설정 파일은 테스트 출력 디렉터리에 격리합니다.

기존 Diagnostics/Verified Graph Virtual LAN 회귀도 통과했습니다. ACTIVE 양쪽 Route와
Edge 1개, Query 실패/LOST/재시작/Diagnostics 비활성화 및 10초 freshness를 확인했습니다.

초기 확장 GUI 테스트는 새 Goal의 목록 반영 전에 이전 Goal을 선택할 수 있어 대기 실패가
있었습니다. 새 handle 반영을 기다리도록 테스트를 보완했습니다.
이후 100ms Timeout probe의 결과 assertion이 1회 실패했습니다. 당시 errno/플래그 로그가
없어 원인은 미확정입니다. 로그 추가 후 4회 연속 통과했으며, 이 결과를 최초부터 모두
통과한 것으로 해석하지 않습니다. 제품 Timeout 기본값은 변경하지 않았습니다.

위 자동 검증 단계 당시에는 물리 LAN과 실제 데스크톱 GUI 수동 검증이 미실행이었습니다.
이후 사용자가 확인한 물리 LAN 결과는 다음 절에 별도로 기록합니다.

## 물리 LAN 수동 검증 — 사용자 확인

다음은 **사용자가 확인한 2026-10-21 수동 검증 결과**입니다. 자동 테스트나 이번 문서 작업에서의 재실행 결과가 아닙니다.

- Framework v5.3 / KCF Tools v0.13
- Orange Pi 5+ ↔ Ubuntu VM Bridged LAN
- 실제 Qt GUI 사용

| 검증 항목 | 사용자 확인 결과 |
| --- | --- |
| Discovery / Metadata | PASS |
| Remote Topic Subscribe 및 실제 Payload 수신 | PASS, 약 45.96Hz 관측 |
| Parameter Get / Set / Readback | PASS |
| Service Call / Response | PASS |
| Action Goal ACK / Feedback / SUCCEEDED Result | PASS |
| Action Cancel ACK / CANCELED Terminal | PASS |

Cancel Result는 `count=-1, enabled=false, gain=1.5`였으며,
`execution_unknown=0`, `cancel_unknown=0`을 확인했습니다.
약 45.96Hz는 해당 환경의 관측값으로, 보장 전송률이나 Application 처리율이 아닙니다.
이 기록을 전체 안정성 인증 또는 모든 재연결 시나리오의 PASS로 해석하지 않습니다.

### 문제 및 해결 이력

- VM UFW가 Discovery UDP `37651`을 차단해 Peer LOST가 발생했습니다.
- 동일 Host ID의 Gateway 중복 실행으로 Endpoint 발견 문제가 발생했습니다.
- Action 예제의 기본 2초 완료 시간으로 Cancel 수동 시험이 어려워,
  Orange Pi에서 테스트용 10초 설정을 사용했습니다. Tool의 Timeout 기본값 변경을 뜻하지 않습니다.
- 위 문제를 정리하고 양쪽 프로그램을 전체 재시작한 뒤 정상 연결 및 Cancel 검증을 완료했습니다.
  Gateway 또는 Tool 한쪽만 재시작한 복구 검증으로 간주하지 않습니다.

## 남은 제한사항

- 물리 LAN에서 Gateway만 재시작했을 때의 자동 재발견·복구는 추가 검증이 필요합니다.
- Tool만 재시작했을 때의 복구도 추가 검증이 필요합니다.
- 일시적인 LAN 단절 후 복구는 추가 검증이 필요합니다.
- 실제 다중 Gateway 동시 제어는 추가 검증이 필요합니다.
- 기존 Action의 자동 재연결과 불확실한 변경 요청의 자동 재시도는 지원하지 않습니다.
- Host ID allowlist는 암호학적 인증이 아닙니다.

위의 Virtual LAN 재시작/Session 테스트 PASS를 물리 LAN의 미검증 복구 항목에 대신 적용하지 않습니다.

<a id="변경-파일"></a>

## Historical — Remote 통합 당시 변경 파일

- 빌드/실행/문서: `CMakeLists.txt`, `src/main.cpp`, `README.md`, `docs/REMOTE_CONTROL.md`
- Backend: `include/kcf_tool/backend/tool_backend.hpp`,
  `include/kcf_tool/backend/kcf_backend.hpp`, `src/backend/kcf_backend_stub.cpp`,
  `src/backend/network_backend.cpp`, `src/backend/remote_runtime.hpp`,
  `src/backend/remote_runtime.cpp`
- Model: `include/kcf_tool/model/network_info.hpp`, `include/kcf_tool/model/remote_control.hpp`
- UI: `include/kcf_tool/ui/main_window.hpp`, `src/ui/main_window.cpp`,
  `include/kcf_tool/ui/graph_widget.hpp`, `src/ui/graph_widget.cpp`,
  `include/kcf_tool/ui/remote_workspace.hpp`, `src/ui/remote_workspace.cpp`
- 테스트: `tests/network_e2e_fixture.cpp`, `tests/remote_workspace.cpp`,
  `tests/remote_control_e2e.py`, `tests/real_smoke.cpp`, `tests/smoke.cpp`

