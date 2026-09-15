# KT-2 — Minimum Required Framework Extensions

[Gap Analysis](KCF_INTERFACE_GAP_ANALYSIS.md)의 12개 요구사항을 충족하는 데 필요한
최소 기능 목록이다. API 서명, SHM 형식, 전송 protocol 또는 구현 지침을 확정하지 않는다.
Framework 변경과 실제 연결은 이번 단계에서 수행하지 않았다.
추천 경계는 KCF가 소유하는 작은 public introspection client API이다.

## R1 — Runtime discovery metadata

- **Why Needed:** 고정 SystemStatus는 aggregate 상태만 제공한다. Supervisor의 element 목록,
  executable, PID, runtime heartbeat/error는 외부에서 열거할 수 없다.
- **Tool Feature Enabled:** application/element 목록, 상태 요약, process 상세 및 연결 상태.
  application identity, element friendly name, executable, PID, 실제 execution mode와 state를
  구분하고 instance 수명/PID 재사용/stale 상태를 설명할 수 있어야 한다.
- **Framework Impact:** application/element 소속과 현재 실행 instance를 외부에 노출하는
  public metadata 계약. supervised와 standalone의 등록/상태 제공 범위를 명확히 해야 한다.
  기존 SystemStatus 접근은 재사용 가능한 하위 기능이다. Supervisor 소유 socket은 노출/공유하지 않는다.
- **Data-path Impact:** process 수명·상태 갱신 중심의 제한된 비용. Tool 장애를 runtime stop,
  heartbeat failure 또는 supervision failure로 처리하지 않아야 한다. Topic publish 경로에는 영향 없어야 한다.
- **Required / Optional / Deferred:** 기본 identity/runtime 상태는 Required (Phase 1).
  별도 process 시작 시각 표시는 Optional. 다중 application 선택 UI는 Deferred지만
  discovery identity 계약은 처음부터 충돌 없이 구분 가능해야 한다.

## R2 — Endpoint registry capability

- **Why Needed:** 이름을 이미 알아야 Create/Open할 수 있으며 topic/parameter 열거와
  publisher/subscriber 관계를 제공하는 public API가 없다.
- **Tool Feature Enabled:** topic/parameter 목록과 이름 lookup, publisher/subscriber 표시,
  R1 instance에 대한 endpoint 소속. empty와 unknown, 제거/재시작된 endpoint를 구분한다.
- **Framework Impact:** endpoint 종류·이름·type identity·owner/role 관계 및 유효성을
  조회할 수 있는 public 계약. registry는 기능 명칭이며 중앙 master/service를 뜻하지 않는다.
- **Data-path Impact:** 생성/접속/종료 같은 수명 변화에 따른 metadata 비용은 허용하되
  매 Publish/Read마다 registry 갱신·조회하거나 Tool 응답을 기다리지 않아야 한다.
- **Required / Optional / Deferred:** Required (Phase 1). network graph/remote registry는 Deferred이며 현재 제안에서 제외.

## R3 — Stable flat type descriptor

- **Why Needed:** sizeof/alignof는 type name이나 필드 의미가 아니다. application header 없이
  generic 값 표시·변환을 하려면 Level 1/2 의미 정보가 필요하다.
- **Tool Feature Enabled:** type 이름과 flat field name/primitive kind/value 표시,
  ParameterInfo 및 DataSnapshot 변환. 필요한 경우 field offset/size와 payload bounds를 검증한다.
- **Framework Impact:** stable type identity/name과 descriptor의 일치/호환성 계약.
  지원 primitive 폭·표현 및 bounded string의 길이/인코딩을 정의하고 미지원 type을 명확히 거부한다.
  descriptor를 소비하는 public API가 private ABI 의존을 흡수하며 Tool에 application header를 요구하지 않는다.
- **Data-path Impact:** 등록 시 고정 metadata 제공, 조회 시 해석. publish마다 reflection,
  string formatting, schema 전송을 추가하지 않는다.
- **Required / Optional / Deferred:** Level 1 및 primitive/string 의미는 Required (Phase 1–2),
  topic flat Level 2는 Required (Phase 3). nested/array/enum, schema language와 IDL은 Deferred/현재 제안 제외.

## R4 — Dynamic topic snapshot access

- **Why Needed:** SharedChannel<T>::Open/ReadLatestSnapshot는 동일 T/ABI를 요구한다.
  기존 slot pin을 단순 노출하면 observer crash가 publisher의 slot 확보에 영향을 줄 수 있다.
- **Tool Feature Enabled:** topic 이름 기반 일관된 latest value와 sequence/descriptor 대응.
  데이터 없음·미존재·권한 오류·미지원·stale/불일치를 구분한다.
- **Framework Impact:** private layout을 숨기는 public dynamic read 기능과 reader 수명/실패 격리.
  관측자의 crash가 control 경로에 pin/lock을 남기지 않는다는 검증 가능한 계약이 필요하다.
  sequence wrap/재시작과 지원 시 timestamp clock 기준을 설명해야 한다.
- **Data-path Impact:** Tool 미실행 시 introspection용 per-publish 변환/대기 없음.
  활성 관측의 복사/동기화 비용은 제한하고 공개해야 한다. 읽기 실패나 느린 관측자로
  control publisher가 무기한 대기하거나 기존 데이터 경로의 가용성을 잃으면 안 된다.
- **Required / Optional / Deferred:** latest read와 crash 격리는 Required (Phase 3).
  sample timestamp 및 frequency는 Optional. 과거 데이터 보관/무손실 recording은 Deferred.

## R5 — Dynamic parameter access and write policy

- **Why Needed:** typed Get/Set은 존재하지만 이름만으로 type/value/writable를 알 수 없다.
  creator 소유권은 Unlink 권한이며 read-only Set 정책이 아니다.
- **Tool Feature Enabled:** parameter metadata/get/set, read-only editor 차단,
  잘못된 값과 권한 오류 표시, 성공한 값의 재조회.
- **Framework Impact:** R2/R3와 연계한 public read/write 및 writable 정책 enforcement.
  Tool-side disable만으로 권한을 구현했다고 보지 않는다. 문자열 입력의 type/range/길이 검증,
  실패/commit 결과, 동시 변경과 재시도 의미가 필요하다. 기존 Set의 commit 후 notify 실패를
  adapter에서 단순 미변경 실패로 번역하면 안 된다.
- **Data-path Impact:** 저주기 설정 경로에서 제한된 검증/동기화 비용. Tool crash로 불완전
  transaction이나 무기한 lock을 남기지 않아야 한다. 정상 parameter write에 따른 의도된
  제어 동작 변화와 introspection 장애의 부작용을 구분한다. Topic 경로에 의존성을 추가하지 않는다.
- **Required / Optional / Deferred:** primitive/string read/write 및 writable 정책은 Required (Phase 2).
  compare-and-set/version 조건부 변경과 parameter watcher의 Tool 노출은 Deferred.

## R6 — Dynamic topic observation lifecycle

- **Why Needed:** Subscriber<T> worker 기능은 있으나 T 없는 observer와 Tool callback 계약은 없다.
- **Tool Feature Enabled:** StartTopicMonitor/StopTopicMonitor와 최신 snapshot callback.
- **Framework Impact:** R3/R4 기반 관측, 시작/중지/중복 등록/빈 callback/오류/예외/receiver 수명,
  진행 중 callback 종료 정책이 필요하다. callback은 GUI thread라고 가정하지 않으며
  유효 기간이 제한된 snapshot을 Tool이 복사해 queued delivery할 수 있어야 한다.
- **Data-path Impact:** observer가 느려도 control producer가 기다리지 않는다. 최신값 중심의
  bounded 관측 비용과 sample skip 의미를 정한다. 등록된 observer가 없으면 추가 worker와
  동적 decode 같은 불필요한 관측 비용을 요구하지 않는다.
- **Required / Optional / Deferred:** generic continuous observation은 Required (Phase 4).
  구체 thread/scheduling은 구현 전 합의. 무손실 queue/recorder는 Deferred.

## 공통 수용 조건

위 기능은 Tool 없이도 KCF가 정상 동작하고, Tool crash/introspection failure가
control path에 영향을 주지 않아야 한다. public API만 감싼 기존 reader로 이 조건이
자동 충족되지는 않는다. 기능별 설계 검증 시 다음 항목을 확인해야 한다.

- reader/observer 강제 종료, 느린 callback, stale instance와 descriptor 불일치가 control을 방해하지 않는지.
- bounded 접근 및 cancel로 UI를 안전하게 갱신할 수 있는지. blocking discovery를 GUI thread에서 호출하지 않는지.
- Framework에 Qt, Tool에 application header/private SHM layout 의존성이 없는지.
- 실패/미지원/unknown과 정상 빈 목록/0 값을 구분할 수 있는지.

이 조건을 public client API로 만족시킬 수 없으면 별도 optional local endpoint 대안을
재평가한다. mandatory daemon, central master, middleware, network/remote 기능을 요구하지 않는다.

## 순서 및 현재 상태

Phase 1 R1/R2 + R3 identity → Phase 2 R3 primitive + R5 → Phase 3 R3 flat fields + R4
→ Phase 4 R6 → Phase 5 Tool field viewer. Graph는 별도 Deferred이다.
각 phase가 완료되기 전에는 해당 Tool 기능을 지원한다고 표시하지 않는다.

현재 KcfBackend는 DISCONNECTED, 실제 operation은 -ENOTSUP이다.
이 목록은 필요한 기능 제안이며 KCF 지원 완료 또는 실제 연결 구현을 뜻하지 않는다.
