# Network Remote Discovery / Graph 관측

현재 Tools 버전은 **0.14.0**, NET56 연동 대상은 **Framework 5.5.0 개발 작업 트리**다.
v5.6.0 정식 릴리스·배포를 선언하지 않는다. 이번 정합성 작업에서는 빌드·시험을 재실행하지 않았다.

## 현재 NET56 구현

- Host ID 미설정은 임시 고유 Observer Identity다. 현재 설정의 `explicit_control_identity`와
  `network_client_available`을 구분하며 저장된 QSettings 값만으로 제어 권한 표시를 켜지 않는다.
- NET56 Core에서 `RemoteTopicClient`와 기존 공유 Discovery를 사용한다. 임시 Observer도
  DISCOVERY / REMOTE_TOPIC_RECEIVER / TARGET_ENDPOINT로 Topic 구독 가능하다.
  Topic Exposure가 허용한 REMOTE Publisher의 VALUE Descriptor는 사전 Host Allowlist 없이 조회한다.
  LOCAL Topic/Subscriber 및 Parameter/Service/Action Descriptor에는 이 자동 권한을 적용하지 않는다.
- 임시 Observer의 Parameter Get/Set·Service·Action은 UI와 Backend에서 허용하지 않는다.
  명시적 Host ID의 기존 Remote Control은 유지하되 서버 Allowlist가 최종 검증한다.
  Host ID·Allowlist는 암호학적 인증이 아니며 신뢰 LAN 전제는 변하지 않는다.
- 새 Session/Endpoint에서 기존 Subscription을 자동 재사용하지 않는다. 사용자 재선택이 필요하다.
  Action 자동 reattach·변경 명령 자동 retry·Payload forwarding은 지원하지 않는다.

### Host-local Gateway IPC (NET56-5B2)

`--gateway-socket` 기본 `/run/kcf/gateway.sock`; `--backend kcf`에서 별도 Worker로 조회한다.
Core `gateway/LOCAL_IPC.md` 계약을 따른다: Unix stream, 16-byte big-endian header,
magic `0x4b43464c`, version 1, operation 1 GET_STATUS / 2 GET_PEERS, 길이는 header 제외 body 길이,
최대 1 MiB. JSON의 64-bit 값은 decimal string이며 native C++ layout을 전송하지 않는다.
버전·필수 필드·크기·부분 송수신·EOF를 검증하고 유한 timeout/Stop으로 자원을 회수한다.
같은 UID가 필요하며 Gateway는 socket mode 0600과 SO_PEERCRED를 검사한다.

Local Gateway Host/Boot/Session, NIC/IP, WAITING_NETWORK/REBINDING/RUNNING/ERROR 및
Peer Endpoint/Metadata Revision을 읽기 전용 별도 Snapshot으로 표시한다. IPC 연결 ERROR와
Gateway Network ERROR는 별개다. 실패한 조회를 최신 정상 데이터로 남기지 않는다.
Direct Observer 결과와 무조건 합치지 않으며 Local IPC 실패가 Direct Discovery를 종료하지 않는다.
Local IPC는 Payload, Parameter/Service/Action 또는 Diagnostics relay가 아니다. 기존 Diagnostics는
Direct Observer 경로를 사용하며 대상 Gateway의 명시적 Diagnostics opt-in이 여전히 필요하다.

```sh
./build/kcf_tool --backend kcf
./build/kcf_tool --backend kcf --gateway-socket /path/to/private-runtime/gateway.sock
# NIC override가 필요하면 인터페이스 이름이 아닌 실제 로컬 IPv4를 사용
./build/kcf_tool --backend kcf --network-interface 192.168.1.10 --network-port 37651
```

`--network-host-id`는 32자리 hex 명시적 Control Identity를 유지한다. 영구 Gateway Host ID를 공유하지 않는다.
Tools는 Gateway 서비스 자동 설치·시작 프로그램이 아니다. Framework의 자동 NIC 재바인딩과
Tools의 자동 NIC failover/Topic 재구독을 혼동하지 않는다(후자는 미구현).

### 구현과 검증의 경계 (2026-10-25 대조)

- **구현 완료:** Temporary Observer와 Local IPC Client/Backend/UI 및 관련 테스트 소스가 존재한다.
  KCF 연결 빌드는 nlohmann_json >= 3.2 CMake 패키지를 요구한다. GUI OFF에도 적용되며
  Framework 없는 Mock/stub는 이 조건부 의존성 없이 유지한다.
- **실제 기록 확인:** 기존 Virtual LAN·Mock/Qt 검증 수치와 실패 이력은 아래 Historical 절의
  당시 버전 기준이다. 보존된 `/tmp/kcf-tools-name/Testing/Temporary/LastTest.log`는
  Explorer identity, Local Graph, Network Graph 3개 Qt 시험 PASS이며 NET56 전체 검증을 대체하지 않는다.
- **STAB-3 당시 증거 미확보:** NET56-5A/5B2 전용 테스트 실행 로그는 이번 문서 대조에서 확보하지 못했다.
  소스 존재를 실행 PASS로 바꾸지 않는다. 이번 작업에서 새로운 Loopback/Virtual LAN/Mock/Qt PASS는 추가하지 않았다.
- **미검증:** NET56 물리 LAN, 실제 systemd 운영, hard real-time 100Hz, 실제 로봇 Fail-safe.
  Framework scheduler/Fresh-start/CommandFreshnessMonitor가 있어도 Tools가 장치 안전을 보장하지 않는다.

Framework Known Issue: Service Endpoint 선택 단계의 간헐적 `-ENOENT`는 원인 미확정이다.
과거 Integration/ControlTraffic 실패 및 재실행 이력은 유지한다. Type Registry EAGAIN은
bounded 복구(50ms 간격 최대 64회) 후 소진 시 중단하며 당시 Service/Action 전용 Registry 복구는 범위 밖이었다.

후속 STAB-6B-3/4에서는 Service Registry 생성 EAGAIN과 Service Request/Response Type 등록 복구를 구현·검증했다.
Service 내부 토큰과 관측 registration_id를 유지하며 Stop/Runtime 종료 시 보류 작업을 취소하고 Worker를 회수한다.
Supervisor 초기 Registry 생성도 bounded 재시도로 복구한다. Type/Service 복구는 최대 64회 제한을 유지하며,
영구 오류 및 소진 후에는 자동 무한 재시도를 하지 않는다. Action 전용 Registry 복구는 여전히 범위 밖이다.

STAB-6B-5의 Graph 준비 조건은 Runtime 이름만이 아니라 RUNNING 및 실제 Subscriber Endpoint를 확인하도록 보완했다.
통제 재현에서 namespace PID 1의 assertion/abort가 SIGSEGV로 나타나는 경로를 backtrace로 확인했지만,
과거 Network Service 선택 `-ENOENT`의 최초 Registry 누락 원인은 미확정으로 유지한다.
STAB-6B-6의 Real Smoke 6.4Hz(Topic 재생성 후) / 24.9Hz(depth4 최초 Echo)도 원인 미확정이다.
진단 추가 후 각 5회 및 CTest 1~10번은 PASS했으나 과거 실패를 해결로 소급하지 않는다.
실제 NET56 LAN·systemd 운영·RT 성능·로봇 Fail-safe는 여전히 미검증이다.
근거는 로컬 `/tmp/kcf-stab6b4`, `/tmp/kcf-stab6b5`, `/tmp/kcf-stab6b6` 기록이며 배포 자료가 아니다.

## Historical — v0.13 / Framework v5.3 Diagnostics 단계

이하 미지원 설명·SHA·검증 기록은 해당 개발 당시 사실이며 현재 NET56 기능 목록이 아니다.
당시 Framework 기준은 v5.3.0 dev 미커밋 working tree (F-ND5B 완료)였고,
검증 출처는 [v0.13 릴리스 노트](V0_13_RELEASE_NOTES.md)에 보존한다.

<a id="v013-network-diagnostics-계약"></a>

## Historical v0.13 Network Diagnostics 계약

- 기존 Observer worker와 실행 중인 `PeerDiscovery`를 재사용한다. 별도 Discovery,
  Gateway, TopicDataPlane, listener를 만들지 않는다. Framework 공개 API만 사용하며
  protocol/codec을 복제하지 않고 Topic payload 수신·forwarding을 하지 않는다.
- ONLINE, 호환 가능, NETWORK_DIAGNOSTICS capability가 있는 Gateway만
  `NetworkDiagnosticsClient::Query()`로 조회한다. Gateway는 `diagnostics_enabled`를
  활성화해야 하며 Framework 기본값은 OFF다.
- 동기식 bounded Query는 GUI가 아닌 worker에서 직렬 실행한다. Tool timeout은 **80 ms**,
  자동 retry는 없다. 이후 정기 관측은 새 Query다. Stop/소멸은 진행 중 Query의 유한 timeout 후 join한다.
- 기존 100 ms Discovery snapshot 갱신을 유지하며 tick당 최대 한 Gateway를 조회한다.
  Gateway별 약 1초를 목표로 순환하고 다수 Host에서는 조회량 제한과 공정성을 우선한다.
  Query 종료 뒤 다음 조회를 예약하므로 고정 1초 주기나 모든 Host의 동시 관측을 보장하지 않는다.
- 성공 후에도 Peer의 ONLINE·Host/boot/session을 다시 확인한다. 이전 통계를 새 세대의
  유효한 값으로 제공하지 않는다. GUI Refresh는 저장된 Backend Snapshot을 복사한다.

### Route 및 Counter

Gateway Host/boot/session과 source/destination 전체 Endpoint identity
(Host/boot/PID/start_ticks/registration), 양쪽 session, route_id, sending 역할을 보존한다.
Topic 이름으로 합치지 않으며 송신/수신 Gateway 통계는 독립적이다. name, state,
error/reason, total_routes, truncated, 성공 관측 시각도 제공한다.

원본 Counter 9개는 `packets_sent`, `packets_received`, `samples_published`,
`local_samples_skipped`, `dropped`, `duplicate`, `out_of_order`, `missed_sequence`, `malformed`다.
정상 빈 Route, capability 미지원, Query 오류, Timeout, LOST 및 truncated를 구분한다.

### Rate와 유효성

동일 Gateway/Route 전체 identity와 양쪽 session·route_id·sending 역할이 일치하는
성공 Snapshot의 Counter Delta를 실제 `steady_clock` 경과 시간으로 나눈 평균이다.
최초는 **Measuring**, 최소 관측 구간은 **0.5초**이며 짧은 성공 관측은 구간을 누적한다.
고정 1초로 나누거나 100 ms Discovery 복사를 새 Diagnostics 관측으로 계산하지 않는다.

| 표시 | Counter |
| --- | --- |
| Network Send Hz | packets_sent |
| Network Receive Hz | packets_received |
| Proxy Publish Hz | samples_published |
| Local Skipped/s | local_samples_skipped |

Proxy Publish는 Gateway의 Local SHM 발행률이며 Application Subscriber 처리율이 아니다.
Skipped는 Network sampling에서 생략된 Local sample이며 packet loss와 같지 않다.
나머지 5개 Counter의 Delta도 모델에 보존한다. 양쪽 Gateway 관측 구간은 독립적이다.

identity/session 변경, LOST, Query 오류/Timeout, Route 종료 또는 어느 Counter든 감소하면
기존 Rate를 무효화하고 새 관측을 시작한다. **Unavailable**과 정상적인 **0 Hz**는 다르다.
truncated에서도 양쪽 연속 관측에 실제 포함된 동일 Route의 Rate는 계산할 수 있다.
누락 Route를 삭제로 단정하지 않지만 연속성은 끊으며, 오래된 Counter와 건너뛰어 연결하지 않는다.

### Detail, Graph 및 호환성

Remote Topic Detail은 Route별 상태·identity·error/reason·Counter·Rate를 표시한다.
Publisher에는 Send/Skipped, Subscriber에는 Receive/Proxy Publish를 표시하고 합산하지 않는다.
[Verified Route Graph](GRAPH.md#v013-verified-network-route-graph)는 양쪽 증거와 10초 최신성을
추가 검사한다. 새로운 자동 GUI polling은 없으며 수동 Refresh 계약을 유지한다.

`network_diagnostics_client.hpp`가 없는 구형 Network Framework는 기존 Discovery-only
관측을 제공한다. Network API도 없는 Framework는 기존 Local-only, Framework 미지정은
Mock/stub이며, 지원하지 않는 통계를 가짜 0으로 만들지 않는다. Remote Set/Call/Goal/Cancel은 미지원이다.

## Historical — v0.12 기록

이하 공개 기준·검증 절의 “현재/이번/미지원”은 당시 v0.12 범위를 뜻한다.
날짜·SHA·측정값을 소급 변경하지 않는다. 현재 실행/lifecycle 및 모델 설명에는 위 v0.13 계약이 추가 적용된다.

2026-09-29 [v0.12](V0_12_RELEASE_NOTES.md)에 포함한 변경이다.
Framework protocol 및 payload codec은 복제하지 않는다.
기존 v0.11 Local Tool 기능 위에 read-only 관측을 추가한다.

## Historical — v0.12 공개 기준

당시 Tool 기준은 **kcf_tools main / v0.12**다.
`f4fb4471fc0b03b24663eb20eede8d8491fd87f6`는 최초 기능 반영 커밋이다. 당시 Network 빌드 기준은
`Nyamkani/kss_control_framework` dev의 **KCF v5.2**
`d41fc2fc71d3b60b0475035ec56ac32b92ffb849`이다. Git tag/GitHub Release 생성 여부는 확인하지 않았다.

아래 미커밋 검토 및 최초 18/18·6/6은 공개 커밋 반영 전의 기록이다. 당시 Framework
Network working tree는 이후 `0a3f2a6`으로 반영됐으며, 더 이상 미커밋 소스가
필수라는 의미가 아니다. 후속 공개 커밋 검증은 별도 절에 기록한다.

## KCF v5.2 d41fc2f 최종 정합성 검증 (2026-09-29)

당시 기준 `d41fc2fc71d3b60b0475035ec56ac32b92ffb849`의 수정 없는 독립 Git archive
추출본(`/tmp/kcf-published-d41fc2f`)으로 새 configure/build 및 전체 회귀를 실행했다.
CMake project version과 실제 Network FrameworkVersion은 **5.2.0**,
NetworkProtocolVersion은 **1.0**이다.

- `tests/network_backend.cpp`: 실제 FrameworkVersion 기대값만 5.1.0 → 5.2.0으로 수정.
  NetworkProtocolVersion 기대값 1.0 및 `network_graph`의 synthetic 5.1.0 표시 데이터는 유지.
- KCF + Qt: **18/18 PASS**, 24.21초. PID/mount namespace 및 별도 `/dev/shm` 사용.
- Framework 없는 Mock/stub + Qt: **6/6 PASS**, 1.27초.
- 로그: `/tmp/kcf-tools-d41-final/Testing/Temporary/LastTest.log`,
  `/tmp/kcf-tools-d41-final-no-kcf/Testing/Temporary/LastTest.log`.
- Framework 원본은 기존 변경을 보존하고, 독립 추출본 215개 파일은 빌드 전후 지정 커밋과
  일치함을 확인했다. Tool 제품 동작, Graph·Remote 기능 및 지원 범위는 변경하지 않았다.
- Tool + Virtual LAN 통합, 실제 물리 LAN 및 Remote GUI 수동 검증은 여전히 미수행이다.
- 이번 작업에서는 commit/push를 수행하지 않았다.

이 결과는 아래 과거 0a3f2a6의 18/18과 별개다. 당시 중간 17/18은 실제 FrameworkVersion
5.2.0에 대해 Tool이 5.1.0을 기대하여 발생했다. 이후 d41fc2f에서 기본값이 5.2.0으로
확정됐고 이번에 기대값을 맞춰 재검증했다. 과거 실패·성공 기록은 삭제하지 않는다.

## 과거 공개 0a3f2a6 후속 검증 (2026-09-29)

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

## Historical — v0.12 route diagnostics의 공개 API 한계

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
위 「Historical — v0.12 공개 기준」을 따른다. 당시 검증 수치와 파일 목록을 보존한다.

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
