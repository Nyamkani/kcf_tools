# Network Remote Discovery / Graph 관측

2026-09-29 [v0.12](V0_12_RELEASE_NOTES.md)에 포함한 변경이다.
Framework protocol 및 payload codec은 복제하지 않는다.
기존 v0.11 Local Tool 기능 위에 read-only 관측을 추가한다.

## 현재 공개 기준

Tool v0.12는 `Nyamkani/kcf_tools` main의
`f4fb4471fc0b03b24663eb20eede8d8491fd87f6`로 반영됐다. 현재 Network 빌드 기준은
`Nyamkani/kss_control_framework` dev의 **KCF v5.2**
`0a3f2a630cec209793f9ac6e99107752e9e01283`이다. Git tag/GitHub Release 생성 여부는 확인하지 않았다.

아래 미커밋 검토 및 최초 18/18·6/6은 공개 커밋 반영 전의 기록이다. 당시 Framework
Network working tree는 이후 위 v5.2 커밋으로 반영됐으며, 더 이상 미커밋 소스가
필수라는 의미가 아니다. 후속 공개 커밋 검증은 별도 절에 기록한다.

## 공개 KCF v5.2 후속 검증 (2026-09-29)

위 최초 working tree 검증과 별도로 공개 커밋
`0a3f2a630cec209793f9ac6e99107752e9e01283`의 **수정 없는 Git archive 추출본**으로
새 configure/build 및 회귀를 실행했다. 추출본 215개 파일은 Git tree의 blob hash와
빌드 전후 일치함을 확인했다. 소스는 `/tmp/kcf-v52-published-0a3f2a6`이며
기존 KCF_SOURCE_DIR 방식으로 참조했다.

- KCF + Qt: **18/18 PASS**, 24.50초. `/tmp/kcf-tools-v52-published`.
- Framework 없는 Mock/stub + Qt: **6/6 PASS**, 3.12초. `/tmp/kcf-tools-v52-review-no-kcf`.
- 로그: 각 빌드의 `Testing/Temporary/LastTest.log`.
- KCF 회귀는 기존과 같이 user/mount/PID namespace 및 별도 `/dev/shm`에서 실행했다.
- 실제 Backend는 loopback, GUI는 offscreen이다. Tool + Framework Virtual LAN 통합,
  물리 LAN 및 데스크톱 Remote 수동 검증은 수행하지 않았다.
- 기능 코드·테스트 기대값·Framework 소스는 수정하지 않았다. CMake는 도움말만 변경했다.

처음 참조한 Framework checkout은 시작 시 clean이었으나 작업 도중 외부 변경이
나타났다. 변경된 `FrameworkVersion` 기본값(5.2.0)과 기존 테스트 기대값(5.1.0)의
불일치로 해당 중간 실행은 17/18이었다. 이 결과를 공개 커밋 결과로 사용하지 않고,
위 독립 추출본으로 새 빌드와 전체 회귀를 완료했다. 공개 `0a3f2a6`의 해당 기본값은
5.1.0이며 문서의 배포 버전과 구분한다. 외부 변경은 되돌리거나 수정하지 않았다.
따라서 원래 Framework working tree의 작업 전후 status가 동일하다고 주장하지 않는다.

GitHub main/dev HEAD는 `git ls-remote`로 위 공개 SHA와 일치함을 확인했다.
이번 문서 정합화에서는 commit/push를 수행하지 않았다.

## 실행 및 lifecycle

```sh
cmake -S . -B build-network \
  -DKCF_SOURCE_DIR=/path/to/kss_control_framework \
  -DKCF_TOOL_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build-network -j4
./build-network/kcf_tool --backend kcf --network-interface 192.168.1.10
```

Qt가 시스템 검색 경로에 없으면 `-DCMAKE_PREFIX_PATH=/path/to/Qt/gcc_64`를 지정한다.
Network 지원 Framework의 `kcf/network/peer_discovery.hpp`가 있어야 관측 기능을
빌드한다. 기존 R5 Local API만 있는 Framework와 Framework 없는 Mock/stub 구성도
유지한다. 후자는 Network unavailable을 표시하며 socket을 만들지 않는다.

`--network-interface` 기본값은 `0.0.0.0`, `--network-port` 기본값은 `37651`이다.
여러 NIC 환경에서는 실제 IPv4 인터페이스 주소를 명시한다. `--no-network`는
Network 관측을 끈다. localhost 자동 검증은 `127.0.0.1`을 사용한다.

Tool은 독립적인 ephemeral Host/boot identity의 DISCOVERY-only observer다.
Gateway의 Host identity를 사칭하지 않는다. 같은 머신에서 실행 중인 Gateway도
Network로 발견된 별도 Host로 표시하며 Local registry 데이터와 합치지 않는다.
다른 peer에서도 endpoint 없는 Tool observer가 보일 수 있다.

`KcfBackend::StartNetwork()`는 worker를 시작하고 즉시 반환한다. socket 시작,
metadata 조회, 변환은 worker에서 수행한다. 비동기 시작 오류는 snapshot status에
기록한다. worker가 Framework `PeerDiscovery`의 `GetPeers()`를 사용하며 자체 wire
protocol은 없다. GUI는 수동 Refresh 때 메모리 snapshot만 복사한다. 자동 fit/refresh는
하지 않는다. Stop/소멸은 worker를 깨우고 join한 뒤 Discovery socket을 정리한다.
Local Refresh로 Network observer를 다시 만들지 않는다. 상단 Connection 표시는 기존
Local registry 연결 상태이며 Remote 관측 상태는 Graph 하단에 별도로 표시한다.

## 모델 및 Graph

- LOCAL: 기존 Runtime/Endpoint identity와 기존 상세/쓰기/Call 기능을 보존한다.
- REMOTE: Host + boot + PID/start_ticks + endpoint registration을 모두 키에 포함한다.
  IP, Host label, Application/Element/Topic 이름은 identity가 아니다.
- Host → Application(Supervisor identity 또는 Standalone) → Runtime → Endpoint를
  표시한다. Application/Runtime은 peer가 광고한 endpoint metadata에서 얻는다.
  endpoint를 광고하지 않는 Runtime이나 별도의 Runtime health 상태는 추정하지 않는다.
- Remote endpoint는 각 registration별 노드다. Topic의 Publisher → endpoint,
  endpoint → Subscriber 방향을 유지한다. 이름이 같은 원격 registration도 합치지 않는다.
- Parameter OWNER/CLIENT와 Service/Action SERVER 등록을 점선 소유 관계로 표현한다.
  Service/Action의 실제 client, Goal, call history는 만들지 않는다.
- Host의 ONLINE/LOST, COMPATIBLE/INCOMPATIBLE reason, Framework/Network 버전은
  텍스트로 표시한다. LOST Host는 Framework peer table에 있는 동안 유지한다.
  Framework는 timeout 시 endpoint metadata를 지우므로 오래된 endpoint는 사라진다.
- session/revision/boot 변경은 새 snapshot으로 교체하며 예전 상세 선택을 지운다.
- Remote 노드 선택/더블클릭/Enter는 Graph 내 읽기 전용 Detail만 연다.
  capabilities(REMOTE_* 및 TARGET_* 포함), type/schema/encoding/size, identity,
  metadata revision/session, scope, snapshot 기준 announce age를 확인할 수 있다.
- endpoint pair compatibility는 공개 `CheckEndpointCompatibility()`로 진단한다.
  Host compatibility와 별도이고 route의 증거가 아니다. 동일 이름/kind의 후보만
  endpoint당 최대 32개 비교하며 한도 도달을 명시한다. ABI/codec 구현 여부를
  Tool이 별도로 추정하지 않는다. 경고가 있는 endpoint는 `[PAIR WARNING]`를 표시한다.
- Actions toggle은 Remote Action metadata를 제어한다. 기존 Local backend의 Action
  discovery/조작 기능을 새로 추가한 것은 아니다.
- Host/Application 필터와 Local/Remote, T/P/S/A toggle은 저장된 snapshot에만 적용한다.
  Zoom/Pan과 명시적 Fit을 유지하고 Refresh나 필터 변경으로 강제 축소하지 않는다.

## route diagnostics의 공개 API 한계

**외부 Gateway route visualization/statistics는 이번 구현에서 미지원이다.**

Framework의 `ControlNode::GetTopicDiagnostics()`는 호출한 ControlNode 객체의
process-local route/statistics만 제공한다. Tool이 다른 Gateway 객체를 참조하거나
다른 process의 진단 snapshot을 가져오는 공개 API는 없다. `PeerDiscovery` metadata에는
active route ID/session, proxy ownership, packet counters가 없다.

따라서 Tool에 forwarding ControlNode를 생성하지 않으며 다음을 명시적으로
조회 불가로 표시한다:

- active route/source/destination 및 proxy publisher 판별
- sent/received/dropped/duplicate/out-of-order/missed-sequence/malformed/error counters
- 실제 remote rate

현재 Graph에는 **cross-host data edge가 없다**. 이름/type이 같거나 compatibility가
성공해도 연결하지 않는다. discovered PUBLISHER는 광고된 역할이며 실제 Application
publisher 또는 Network proxy인지를 보장하지 않는다. Detail에 proxy 판별 불가를 표시한다.
통계 0, 가짜 ACTIVE route, 모의 proxy를 실제 관측으로 표시하지 않는다.

추후 Framework에 다른 Gateway의 읽기 전용 관측 API가 제공되면 route ID와 양쪽
Host/session/Endpoint를 확인하고 ACTIVE이며 최신 ONLINE인 route만 그리는 adapter가
필요하다. 이번 작업은 Framework 코드를 변경하지 않는다.

## 최초 검증 기록 (공개 커밋 반영 전)

이번 작업의 자동 검증 결과는 아래에 기록한다. Framework의 기존 Virtual 2-host LAN
PASS는 Framework 기능 검증 이력이며 Tool 통합이나 실제 물리 LAN 검증으로 간주하지 않는다.

- KCF + Qt 구성: 전체 18/18 PASS (기존 16개 + Network Backend/Graph 2개).
- Framework 없는 Mock/stub + Qt 구성: 전체 6/6 PASS (기존 5개 + Network Graph).
- 실제 loopback Backend: 두 peer, 같은 이름/동일 PID/start_ticks의 Host 분리,
  버전/capability, type mismatch, revision 교체, ONLINE → LOST → 새 session ONLINE,
  incompatible peer 표시, 시작 오류, Stop/join.
- Remote Graph/GUI: Host 계층, 이름 충돌 분리, LOST/incompatible 텍스트, T/P/S/A
  ownership, same-name/no-route에서 cross-host edge 없음, endpoint 제거, 필터/toggle,
  Local activation 금지, read-only detail, Zoom/Pan 유지.
- 실제 ACTIVE route 및 proxy/statistics 통합 검증: 조회 API 미제공으로 미검증.
- Tool + Virtual LAN fixture 통합 및 실제 두 물리 장치/데스크톱 수동 조작: 미검증.

재현 명령 (테스트는 root나 network namespace를 필수로 요구하지 않는다):

```sh
ctest --test-dir build-network --output-on-failure
```

기존 Local 테스트는 다른 Runtime이 없는 환경을 전제로 한다. 사용자 IPC를 건드리지
않으려면 허용된 Linux user/mount/PID namespace에서 다음처럼 실행한다:

```sh
unshare --user --map-root-user --mount --pid --fork --mount-proc sh -c \
  'mount -t tmpfs tmpfs /dev/shm && ctest --test-dir build-network --output-on-failure'
```

이번 실제 빌드 경로는 `/tmp/kcf-tools-network`, `/tmp/kcf-tools-network-no-kcf`이며
CTest 상세 로그는 각 `Testing/Temporary/LastTest.log`에 있다.

### 2026-09-29 미커밋 변경 재검토

이 절은 commit 전 검토 당시 기록이다. 이후 Tool main 및 Framework dev 반영 상태는
위 「현재 공개 기준」을 따른다. 당시 검증 수치와 파일 목록을 보존한다.

검토에 사용한 Framework 경로는
`/home/kssvm/workspace/kcf/kss_control_framework`이며 HEAD는
`8360b6e053472ba20acdfc62b7489e33122997ec`이다. 해당 checkout에는 검토 시작 전부터
Network/Action 및 기존 Core 파일의 미커밋 변경이 있었다. 따라서 Network 빌드 결과는
이 SHA의 clean checkout 검증이 아니라 **당시 Network 확장이 포함된 working tree**
검증이다. 이전 workspace의 Framework 변경 작성자를 현재 diff만으로 확정할 수 없다.
이번 Tool 검토에서는 Framework 소스를 수정하지 않는다.

Remote 선택 해제 또는 Local 노드 선택 후 이전 Remote Detail이 남는 문제를 기존
`network_graph` 테스트에서 재현했다. 선택 변경 시 상세를 비우도록 최소 수정하고,
선택 해제 및 Local 선택의 회귀 검증을 추가했다. Local Graph 문서의 Backend 무수정
표현도 Local Graph 범위로 한정하여 Network Backend 확장과 구분했다.

수정 후 재빌드 및 전체 회귀 결과:

- KCF + Qt: **18/18 PASS**, 24.40초. 별도 PID/SHM namespace 사용.
- Framework 없는 Mock/stub + Qt: **6/6 PASS**, 1.28초.
- Framework tracked/untracked 파일 215개의 검토 중 기록한 SHA-256 비교: 변경 없음.
- 실제 물리 LAN 및 데스크톱 수동 검증은 이번에도 수행하지 않았다.
- commit/push는 수행하지 않았다.

당시 Tool 미커밋 변경 파일 전체(기존 수정 10개, 신규 13개):

- `CMakeLists.txt`
- `README.md`
- `docs/GRAPH.md`
- `docs/NETWORK_OBSERVATION.md`
- `include/kcf_tool/backend/kcf_backend.hpp`
- `include/kcf_tool/backend/tool_backend.hpp`
- `include/kcf_tool/model/network_info.hpp`
- `include/kcf_tool/ui/graph_model.hpp`
- `include/kcf_tool/ui/graph_widget.hpp`
- `include/kcf_tool/ui/main_window.hpp`
- `src/backend/kcf_backend_stub.cpp`
- `src/backend/network_backend.cpp`
- `src/main.cpp`
- `src/ui/graph_model.cpp`
- `src/ui/graph_widget.cpp`
- `src/ui/main_window.cpp`
- `src/ui/network_graph_model.cpp`
- `tests/graph_model.cpp`
- `tests/graph_smoke.cpp`
- `tests/network_backend.cpp`
- `tests/network_graph.cpp`
- `tests/real_smoke.cpp`
- `tests/smoke.cpp`

이번 검토에서 추가 수정한 파일은 `src/ui/graph_widget.cpp`, `tests/network_graph.cpp`,
`docs/GRAPH.md`, `docs/NETWORK_OBSERVATION.md` 네 개다.


물리 LAN 수동 확인 시 두 장치의 기존 Gateway를 동일 multicast group/port로 시작하고
각각 고유 Host identity와 정상 endpoint metadata를 광고한다. Tool을 해당 NIC로 실행해
수동 Refresh로 두 Host의 metadata를 확인한다. 한 Gateway 종료 후 약 4초가 지나면
Refresh에서 LOST/endpoint 제거를 확인하고, 재시작 후 새 session/revision을 확인한다.
Tool 종료 후에도 기존 장치 간 통신이 계속되는지 별도로 관측한다. 이 절차를 이번에
실행했다고 주장하지 않는다.

Remote Parameter Set, Service Call, Action Goal/Cancel, call history UI, payload
forwarding은 미지원이며 Tool은 Gateway/Data Broker로 동작하지 않는다.

## 최초 구현 당시 변경 파일

신규:

- `include/kcf_tool/model/network_info.hpp`
- `src/backend/network_backend.cpp`
- `src/ui/network_graph_model.cpp`
- `tests/network_backend.cpp`
- `tests/network_graph.cpp`
- `docs/NETWORK_OBSERVATION.md`

기존 작업 파일 보완:

- `CMakeLists.txt`, `README.md`, `docs/GRAPH.md`
- `include/kcf_tool/backend/tool_backend.hpp`, `include/kcf_tool/backend/kcf_backend.hpp`
- `src/backend/kcf_backend_stub.cpp`, `src/main.cpp`, `src/ui/main_window.cpp`
- `include/kcf_tool/ui/graph_model.hpp`, `include/kcf_tool/ui/graph_widget.hpp`
- `src/ui/graph_widget.cpp`

작업 전부터 있던 Local Graph 변경은 보존했다. Framework/Application/protocol 코드는
이번 작업에서 수정하지 않았으며 commit/push를 수행하지 않았다.
