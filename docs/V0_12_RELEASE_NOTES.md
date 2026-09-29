# KCF Tool v0.12

2026-09-29 현재 작업 상태를 **v0.12**로 지정한다.
[v0.1](V0_1_RELEASE_NOTES.md)과 [v0.11](V0_11_RELEASE_NOTES.md)은 기존 이력으로 보존한다.

## 포함 기능

- 기존 Application/Element Explorer, Topic Echo, Parameter Editor, Service Call 유지.
- Local Graph: Runtime identity 기반 Application·Standalone 그룹, 모든 Publisher/Subscriber registration, 발견된 Parameter OWNER/CLIENT 및 Service SERVER 관계.
- Qt QGraphicsView/Scene, deterministic layout, 필터·toggle, Zoom/Pan/Fit, 기존 Element/Topic 화면 연결. 외부 Graphviz 의존성 없음.
- Remote Discovery: 공개 PeerDiscovery API의 worker snapshot을 수동 Refresh로 표시.
- Host/boot/Runtime/registration identity 기반 Remote Graph와 읽기 전용 상세. 버전·호환성·capabilities·type·ONLINE/LOST 및 T/P/S/A metadata 표시.
- Remote 선택 해제·Local 선택 후 이전 Remote 상세가 남는 문제 수정 및 회귀 검증.

## Framework 및 빌드 기준

`KCF_SOURCE_DIR` 외부 참조 방식을 유지한다. Network 관측은 KCF v5.2의 공개
`kcf/network/peer_discovery.hpp`를 제공하는 Framework가 필요하다.
Network API가 없는 기존 Framework에서는 Local 기능을, Framework 없는 구성에서는
Mock/disconnected stub을 제공한다. `--no-network`로 관측을 끌 수 있다.

현재 빌드 기준은 `Nyamkani/kss_control_framework` dev에 공개된 KCF v5.2
`d41fc2fc71d3b60b0475035ec56ac32b92ffb849`이다. Tool v0.12와 Framework v5.2 버전은 독립적이다.

2026-09-29 최초 Network 검증은 `/home/kssvm/workspace/kcf/kss_control_framework`의
`8360b6e053472ba20acdfc62b7489e33122997ec` HEAD와 Network/Action 및 Core 미커밋
확장을 포함한 working tree에서 수행했다. 당시 Framework 215개 파일 해시는 유지됐다.
그 Network 구현은 이후 `0a3f2a6`으로 dev에 반영됐으며, `d41fc2f`에서
CMake project 및 Network FrameworkVersion 기본값이 5.2.0으로 확정됐다. 아래 최초 검증 결과는 당시
working tree의 기록이며 공개 v5.2 커밋에서 실행한 결과로 소급 변경하지 않는다.

빌드·실행은 [README](../README.md), Network 설정은
[Network 관측](NETWORK_OBSERVATION.md)을 따른다. Framework 교체 후에는 실행 중인
GUI를 종료하고 새 바이너리로 재시작한다. 구버전 GUI 혼용의 Protocol error는
[기존 진단 문서](V51_MANUAL_RECONNECT_DIAGNOSIS.md)를 참고한다.

## 최초 검증 결과 (공개 커밋 반영 전)

| 구분 | 2026-09-29 검토 결과 |
| --- | --- |
| KCF + Qt 재빌드·자동 회귀 | **18/18 PASS**, PID/SHM namespace 격리 |
| Framework 없는 Mock/stub + Qt | **6/6 PASS** |
| Network Backend | loopback peer discovery, identity 분리, compatibility, revision/session, LOST, 시작 오류 및 종료 |
| Graph/GUI | Qt offscreen: 모델·필터·기존 화면 연결·Remote 상세 및 선택 해제 회귀 |
| 실제 물리 LAN·데스크톱 수동 조작 | **미검증** |

로그는 `/tmp/kcf-tools-network/Testing/Temporary/LastTest.log`와
`/tmp/kcf-tools-network-no-kcf/Testing/Temporary/LastTest.log`에 있다.
v0.11의 **14/14 자동 PASS 및 사용자 GUI Echo 값 갱신·SHM 재연결 확인**은 과거의
별도 검증 기록이며, v0.12 Remote 기능의 수동 확인으로 간주하지 않는다.
최초 버전 문서 정리에서는 기능 수정이나 재테스트를 수행하지 않았다.

## KCF v5.2 d41fc2f 최종 정합성 검증 (2026-09-29)

현재 기준 `d41fc2fc71d3b60b0475035ec56ac32b92ffb849`의 수정 없는 독립 Git archive
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

위 최초 검증과 별도로 `0a3f2a630cec209793f9ac6e99107752e9e01283`의 수정 없는
Git archive 추출본에서 새 configure/build를 수행했다.

- KCF + Qt 전체 회귀 **18/18 PASS** (24.50초), PID/SHM namespace 격리.
- Framework 없는 Mock/stub + Qt **6/6 PASS** (3.12초).
- 로그: `/tmp/kcf-tools-v52-published/Testing/Temporary/LastTest.log`,
  `/tmp/kcf-tools-v52-review-no-kcf/Testing/Temporary/LastTest.log`.
- 추출본 215개 파일은 지정 커밋과 일치하며 Framework 및 Tool 기능 코드는 수정하지 않았다.
- 검증 중 원래 Framework checkout에 외부 변경이 나타나 독립 추출본을 사용했다.
  중간 실행의 버전 기대값 불일치와 제외 사유는 [Network 기록](NETWORK_OBSERVATION.md)을 따른다.
- loopback/offscreen 자동 검증이며 Virtual LAN 통합·물리 LAN·Remote 수동 확인을 뜻하지 않는다.

## 제한 및 배포 상태

- 외부 Gateway의 active route/statistics 및 proxy 판별 공개 API가 없어 미지원이다.
- 같은 이름/type만으로 cross-host 전송 관계를 추정하지 않는다.
- Remote Set/Call/Goal/Cancel, payload forwarding, Launch 및 dynamic callback monitor는 미지원이다.
- Remote Action은 metadata 관측만 제공하며 Local Action 기능을 추가하지 않는다.
- 현재 Tool 기준은 **kcf_tools main / v0.12**다. `f4fb4471fc0b03b24663eb20eede8d8491fd87f6`는 최초 기능 반영 커밋이다. Framework 현재 기준은 위 d41fc2f다.
- Git tag 및 GitHub Release 생성 여부는 이번 작업에서 확인하지 않았다. 이번 문서 정합화 작업에서는 commit/push를 수행하지 않는다.
- Tool + Framework Virtual LAN 통합 및 실제 물리 LAN·데스크톱 Remote 수동 검증은 미수행이다.

세부 구현·제한은 [Graph](GRAPH.md), [Network 검토 기록](NETWORK_OBSERVATION.md),
누적 변경은 [Changelog](../Changelog.md)를 따른다.
