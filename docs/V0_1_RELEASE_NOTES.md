# KCF Tool v0.1

KT-8 Generic Cross-Application Validation을 완료한 현재 작업 상태를
**KCF Tool v0.1**로 지정한다. KT-0–KT-8 및 KT-7.1 hardening이 포함된다.
이 버전 지정은 Tool의 기준이며, 외부 KSS Control Framework의 버전을 변경하지 않는다.

## 포함 기능

- Linux C++17 / Qt6 Widgets 독립 GUI, MockBackend 및 선택적 실제 KcfBackend.
- Supervisor별 Application/Element 탐색과 Standalone 분류, PID + start_ticks membership.
- 공개 descriptor 기반 Topic latest Echo 및 100 ms polling, persistent reader와 재생성 감지.
- Parameter scalar·fixed array 조회/편집, 입력 검증, Apply/Revert 및 쓰기 후 재조회.
- Service request form 생성, 명시적 비동기 Call 및 response 표시.
- Runtime 세대와 endpoint registration 검증, stale 접근 거절 및 Refresh 후 재선택.
- Application별 상태 격리와 같은 이름의 Supervisor instance 구분.

## 검증 기준

| 항목 | 결과 |
| --- | --- |
| KT-8 신규 Application 2개 + Standalone 통합 검증 | 1/1 PASS |
| 기존 Tool 회귀 | 10/10 PASS |
| KCF-less GUI | 3/3 PASS |
| KCF-less backend | 1/1 PASS |
| Framework 회귀 | 기존 runner 19개 항목 + scope/supervision 2개 PASS |

서로 다른 schema의 Echo, Parameter typed readback/watcher, Service Call,
동일 이름 Application, 상태·IPC 격리, Reset, endpoint 재등록, 종료·재시작 및
stale 오류 처리를 확인했다. KT-8에서는 Tool production 코드와 KCF core 변경 없이
검증했다. 기존 회귀는 KT-8 프로세스 종료 후 순차 실행했다.

이 기록은 완료된 KT-8 검증 결과를 버전 기준으로 채택한 것이며,
이번 문서 작성에서 테스트를 새로 실행한 것은 아니다.

## 호환성 및 실행

실제 backend에는 Framework **R1–R5 + R4.1 + R2B.1**이 필요하다.
검증 대상은 `/tmp/kcf-r1-introspection`의 `feature/introspection` 개발 상태이며,
배포용 정확한 Framework commit/tag는 아직 확정하지 않았다.

- SHM format: **3**, Service protocol: **2**.
- Supervisor와 child는 동일한 호환 Framework revision으로 rebuild/restart한다.
- child 외부 SystemStatus observer는 `OpenForApplication(pid, start_ticks)`를 사용한다.
- KCF 없이 Mock GUI와 backend-only 빌드가 가능하다.
- 빌드, GUI 실행 및 KT-8 자동/수동 검증 방법은 [README](../README.md)를 따른다.
- 독립 검증 프로젝트 `/tmp/kcf_generic_validation`은 Tool 저장소에 포함되지 않은
  로컬 프로젝트이다. `/tmp` 소스·빌드·로그의 영구 보존은 보장되지 않는다.

## 알려진 제한과 제외 범위

- 검증은 하드웨어 없는 로컬 SHM/loopback UDP와 Qt offscreen GUI를 사용했다.
  실제 하드웨어 동작과 수동 시각 검토는 포함하지 않는다.
- global Topic SHM ownership 때문에 동일 logical Topic의 복수 Publisher 검증은
  수행하지 않았다. 동일 이름 Application 검증에는 별도 endpoint prefix를 사용했다.
- Echo에서 stale/재생성 오류가 발생하면 Refresh 후 새 endpoint를 선택해 시작한다.
- Service timeout은 callback 미실행을 보장하지 않는다. 자동 재호출은 하지 않는다.
- registration 검증은 원자적이지 않으며 Service protocol에 registration_id가 없어
  마지막 검증 직후 재등록되는 race를 완전히 차단하지 못한다.
- Graph, Launch manager, plotting/history, Parameter 자동 watch,
  Dynamic callback Monitor, Service traffic monitor, Action 및 remote control은 제외한다.

## 버전 기록

현재 상태를 문서상 v0.1로 지정했다. git tag 생성, commit/push 및 배포는
수행하지 않았으며, 기존 작업 변경은 보존했다.

- [누적 Changelog](../Changelog.md)
- [KT-7.1 Hardening](KT7_1_PRE_V01_HARDENING.md)
- [KT-8 상세 검증 결과](KT8_GENERIC_CROSS_APPLICATION_VALIDATION.md)
