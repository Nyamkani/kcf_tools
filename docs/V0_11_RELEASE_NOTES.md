# KCF Tool v0.11

현재 상태를 **KCF Framework v5.1 연동 검증 완료 버전 v0.11**로 지정한다.
[v0.1](V0_1_RELEASE_NOTES.md)은 기존 개발 이력으로 보존한다.

## Framework 기준과 동작

- 저장소/브랜치: `Nyamkani/kss_control_framework`, `dev`.
- 검증 commit: `8360b6e053472ba20acdfc62b7489e33122997ec`.
- Topic Format **4**, Parameter Format **3**, Service protocol **2**.
- 기존 `KCF_SOURCE_DIR` 외부 참조 방식을 유지한다.
- Depth=1/4 Topic은 `DynamicTopicReader::ReadLatest()`로 최신값을 읽는다.
  ReadNext 사용이나 Subscriber cursor 소비 기능을 추가하지 않았다.
- Application/Element 탐색, Parameter 편집, Service Call 등 v0.1 기능을 유지한다.

## 검증 결과

| 구분 | 결과 |
| --- | --- |
| 빌드 | 지정된 v5.1 Core로 새 디렉터리에서 Qt GUI 및 테스트 빌드 성공 |
| 자동 테스트 | **14/14 PASS** — Backend, codec, registration, Echo, 지연·반복 재연결, Parameter, Service, Explorer |
| Depth=1/4 자동 확인 | 최신 payload·필드·Sequence, SHM 재생성 STALE 감지 및 재연결 PASS |
| 자동 GUI | Qt offscreen smoke PASS |
| 실제 GUI 수동 확인 | **사용자 확인 완료** — 값 갱신, SHM 재생성 후 Refresh·Publisher 재선택·Start Echo 재연결 |

자동 테스트와 사용자 수동 확인은 별도 결과다. 기존 검증 기록을 반영했으며
이번 버전 문서 정리에서는 기능 코드를 수정하거나 테스트를 다시 실행하지 않았다.
자동 실행 로그: `/tmp/kcf-tools-v51-8360b6e/Testing/Temporary/LastTest.log`.

## Protocol error 진단과 실행 주의

수동 재연결 과정의 Protocol error는 실행 중인 **구버전 GUI 혼용**이 원인이었다.
구버전의 `GetType → ListTypes → SharedChannel<TypeRegistrySnapshot>::Open()`이
예상 파일 크기 2,789,680 bytes와 v5.1의 실제 2,789,688 bytes 불일치로
`-EPROTO`를 반환했다. application Topic SHM Open 전 단계의 실패이며,
현재 Framework/Tool 재연결 구현의 결함이 아니었다.

Framework를 교체해 빌드한 뒤에는 실행 중인 Tool도 종료하고 새 바이너리로
재시작해야 한다. Refresh는 실행 바이너리를 교체하지 않는다.
정확한 근거와 비교 재현은 [수동 재연결 진단](V51_MANUAL_RECONNECT_DIAGNOSIS.md)을 따른다.

## 범위와 배포 상태

v0.1의 기능 범위 및 알려진 제한을 유지한다. 실제 하드웨어 동작 검증이나
새 Graph/Launch/Monitor 기능을 포함하는 버전이 아니다.
이번 버전 지정에서 Git tag, GitHub Release, commit/push는 생성·수행하지 않았다.
문서상 버전 지정이 배포 완료를 뜻하지 않는다.

- [빌드 및 실행](../README.md)
- [Changelog](../Changelog.md)
