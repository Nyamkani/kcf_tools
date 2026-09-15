# KT-2 — KCF External Interface Gap Analysis

## 범위와 판정 기준

기준: [KT-1 요구사항](KCF_EXTERNAL_INTERFACE_REQUIREMENTS.md).
조사 대상은 인접 `../../kss_control_framework` checkout이며 HEAD는
`234716b716f2f7e37c806bb9430dc9a9e11f74ab`이다. 실제 working tree를 읽었다.
시작 시 기존 변경은 `CMakeLists.txt`, `applications/mecanum/data/motor.hpp`,
untracked `applications/mecanum/CMakeLists.txt`, `applications/mecanum/motor/`였다.
이 파일들은 분석 대상으로 읽거나 수정하지 않았다.

요청한 ipc/parameter/process/system header, process_runtime.cpp, bringup의
bringup.hpp/cpp를 조사했다. Supervisor socket 소유 관계 확인을 위해 직접 관련된
process.hpp와 process.cpp의 socketpair/상태 요청 부분만 추가 확인했다.
저장소 전체 감사나 실행 중 application 조사, IPC 접속은 수행하지 않았다.
이하 MISSING은 이 조사 범위에서 해당 public capability를 확인하지 못했다는 뜻이다.

- AVAILABLE: application-specific T나 private layout 없이 외부 Tool 요구를 충족한다.
- PARTIAL: 관련 기능/데이터는 있으나 외부 generic Tool 요구 전체를 충족하지 못한다.
- MISSING: 필요한 discovery/관계 등 기능 자체를 확인하지 못했다.
- DEFERRED: 합의된 후속 범위이다. 필수 12개를 미루기 위한 분류가 아니다.

필수 요구사항 기준 **AVAILABLE 0 / PARTIAL 9 / MISSING 3 / DEFERRED 0**.
별도로 고정 SystemStatus의 현재 상태 읽기/관측은 AVAILABLE인 하위 기능이다.

## 코드 근거

아래 경로는 이 문서 기준 인접 checkout 상대 경로이며, 행 번호는 조사 시점 기준이다.

| 근거 | 위치와 확인 내용 |
| --- | --- |
| S1 | [shared_channel.hpp](../../kss_control_framework/kcf/include/kcf/ipc/shared_channel.hpp), 22–56: 동일 T/ABI 전제 및 private Storage; 87–127: typed Open; 130–166: Publish; 190–225: typed snapshot와 slot pin |
| S2 | [publisher.hpp](../../kss_control_framework/kcf/include/kcf/ipc/publisher.hpp), Publisher::Create/Publish; [subscriber.hpp](../../kss_control_framework/kcf/include/kcf/ipc/subscriber.hpp), Subscriber::Create/Close: T 기반 worker callback |
| S3 | [shared_parameter.hpp](../../kss_control_framework/kcf/include/kcf/parameter/shared_parameter.hpp), 21–46: T/ABI와 private Storage; 76–115: Open; 118–150: Get/Set; 206–228: owner-only Unlink와 이름 인코딩 |
| S4 | [parameter.hpp](../../kss_control_framework/kcf/include/kcf/parameter/parameter.hpp), 27–49: typed Create/Open/Get/Set; 67–108: 선택적 watcher |
| S5 | [process_runtime.hpp](../../kss_control_framework/kcf/include/kcf/process/process_runtime.hpp), ProcessRuntime public/private; [runtime_supervision.hpp](../../kss_control_framework/kcf/include/kcf/process/runtime_supervision.hpp), RuntimeStatusResponse; [process_runtime.cpp](../../kss_control_framework/kcf/src/process_runtime.cpp), 221–288: inherited FD, status response, SupervisorLost |
| S6 | [lifecycle.hpp](../../kss_control_framework/kcf/include/kcf/process/lifecycle.hpp), ProcessState; [execution_mode.hpp](../../kss_control_framework/kcf/include/kcf/process/execution_mode.hpp), DetectLaunchExecutionMode |
| S7 | [system_status.hpp](../../kss_control_framework/kcf/include/kcf/system/system_status.hpp), SYSTEM_STATUS_STORAGE/SystemStatus; [system_status_channel.hpp](../../kss_control_framework/kcf/include/kcf/system/system_status_channel.hpp), 25–70: public Open/ReadCurrent/worker callback |
| S8 | [bringup.hpp](../../kss_control_framework/bringup/include/bringup/bringup.hpp), 25–72: ElementSpec, ManagedElementStatus, private ManagedElement; [bringup.cpp](../../kss_control_framework/bringup/src/bringup.cpp), 205–244: aggregate status publication and local element getter; PollRuntimeStatuses: private last response |
| S9 | [process.hpp](../../kss_control_framework/kcf/include/kcf/process/process.hpp), 30–49: owning Process methods/state; [process.cpp](../../kss_control_framework/kcf/src/process.cpp), 38–41: socketpair and environment; RequestRuntimeStatus/ReceiveRuntimeStatus: owning channel request/response |

## 12개 요구사항 판정

### 1. Application discovery — PARTIAL

- **Requirement:** 이름을 모르는 외부 Tool이 현재 application을 발견하고 식별한다.
- **Current KCF Support:** S7의 고정 `/kcf/system/status/state`와 SystemStatusSubscriber는 application 상태/실패 요약을 제공한다.
- **External Tool Usability:** public `Open()`이 고정 이름을 내부적으로 사용하므로 상태 접근 자체는 가능하다. application 이름을 인자로 알 필요는 없지만, 이 고정 endpoint가 있다는 사전 계약은 필요하다.
- **Gap:** SystemStatus에 application 이름/instance 목록이 없다. 고정 한 상태 endpoint 접근은 application 열거가 아니며 다중 application 식별도 해결하지 않는다.
- **Recommended Minimal Extension:** R1의 application identity와 실행 instance 열거, 상태 endpoint 대응.
- **Priority:** P1 / Required.

### 2. Element discovery — PARTIAL

- **Requirement:** application별 element를 열거한다.
- **Current KCF Support:** S8의 ElementSpec/name, private elements_, local GetElementStatuses가 있다.
- **External Tool Usability:** Supervisor 객체를 가진 동일 프로세스에서만 getter가 유효하다. 별도 Tool이 해당 객체를 얻는 public 외부 접근은 없다.
- **Gap:** 외부 목록, application 소속 및 standalone element 등록/수명 정보가 없다.
- **Recommended Minimal Extension:** R1의 element identity 목록과 instance 유효성.
- **Priority:** P1 / Required.

### 3. Topic discovery — MISSING

- **Requirement:** topic 이름과 metadata를 열거한다.
- **Current KCF Support:** S1/S2는 호출자가 이름을 제공해 Create/Open한다.
- **External Tool Usability:** 이미 알려진 이름과 T로 접근 가능하지만 목록 조회 API는 없다.
- **Gap:** endpoint registry와 application/element 소속이 없다. OS 저장 이름 스캔은 public discovery 계약이 아니다.
- **Recommended Minimal Extension:** R2의 topic enumeration/lookup.
- **Priority:** P1 / Required.

### 4. Publisher / Subscriber relation — MISSING

- **Requirement:** topic별 publisher/subscriber element를 구분한다.
- **Current KCF Support:** S1 private owner_pid는 creator PID이고 S2 subscriber는 로컬 channel/worker를 가진다.
- **External Tool Usability:** owner PID만으로 element friendly name, application 또는 subscriber 목록을 알 수 없다.
- **Gap:** 외부 관계 목록이 없으며 subscriber의 공유 slot reader count는 식별 가능한 subscriber 등록부가 아니다.
- **Recommended Minimal Extension:** R2의 endpoint role과 R1 instance 연결, 종료/오래된 관계 구분.
- **Priority:** P1 / Required.

### 5. Topic type metadata — PARTIAL

- **Requirement:** payload의 type 이름과 flat primitive/string 필드를 해석한다.
- **Current KCF Support:** S1의 size/alignment는 Level 0 layout 검증 정보이다.
- **External Tool Usability:** public semantic type/field descriptor가 없다. 같은 크기와 정렬은 같은 type이라는 증거가 아니다.
- **Gap:** Level 1 stable type identity/name 및 Level 2 field descriptor가 없다.
- **Recommended Minimal Extension:** R3의 작은 flat descriptor와 안전한 해석 계약. Level 3은 제외한다.
- **Priority:** P1 metadata 계약 / P3 topic 활용 / Required.

### 6. Topic latest data read — PARTIAL

- **Requirement:** 이름만으로 일관된 최신 sample과 필드 값을 읽는다.
- **Current KCF Support:** S1 `ReadLatestSnapshot(T&, uint32_t&)`는 payload와 대응 sequence를 복사한다. 아직 publish가 없으면 -EAGAIN이다.
- **External Tool Usability:** T 없는 `SharedChannel<T>::Open(name)`과 T 없는 snapshot overload는 없다.
- **Gap:** dynamic read, descriptor 대응, 관측자 crash 격리가 없다. payload timestamp/frequency도 자동 제공되지 않는다.
- **Recommended Minimal Extension:** R4의 안전한 generic snapshot, R3 descriptor 및 오류/sequence 의미.
- **Priority:** P3 / Required.

### 7. Topic continuous observation — PARTIAL

- **Requirement:** generic callback 기반 start/stop 관측.
- **Current KCF Support:** S2의 Subscriber<T>는 Wait→ReadLatestSnapshot 후 local copy를 worker callback에 전달한다. Close는 StopWait와 join을 수행하고 callback 내부 Close는 -EDEADLK이다.
- **External Tool Usability:** 알려진 T에서는 public typed 관측이 가능하다. callback에는 sequence가 직접 전달되지 않으며 모든 중간 sample 전달을 보장하는 queue가 아니다.
- **Gap:** T 없는 관측, callback 수명/stop/in-flight 계약과 Tool crash 격리가 필요하다.
- **Recommended Minimal Extension:** R6의 dynamic latest 관측. 무손실 recorder는 범위 밖이다.
- **Priority:** P4 / Required, 세부 scheduling은 후속 합의.

### 8. Parameter discovery — MISSING

- **Requirement:** application의 parameter 이름을 열거한다.
- **Current KCF Support:** S3/S4는 이름을 받아 typed Create/Open한다.
- **External Tool Usability:** 목록 조회 API가 없고 이름 인코딩은 저장 구현이다.
- **Gap:** parameter registry 및 owner/application 대응이 없다.
- **Recommended Minimal Extension:** R2의 parameter enumeration/lookup.
- **Priority:** P1 / Required.

### 9. Parameter type / value / writable metadata — PARTIAL

- **Requirement:** name, type, value, writable를 generic하게 조회한다.
- **Current KCF Support:** S3는 size/alignment/owner/version 및 typed value를 보유한다.
- **External Tool Usability:** T를 알아야 값을 읽는다. stable semantic type와 writable 조회 API가 없다.
- **Gap:** `owns_name_`는 Unlink 권한이지 Set 허가가 아니다. mock의 read-only 정책은 현재 KCF 기능을 증명하지 않는다.
- **Recommended Minimal Extension:** R3 type 정보와 R5 명시적 writable 정책/권한 및 current value.
- **Priority:** P2 / Required.

### 10. Parameter get / set — PARTIAL

- **Requirement:** 이름 기반 generic read/write와 결과 확인.
- **Current KCF Support:** S4 `Get(T&, uint64_t*)`, `Set(const T&)`; S3 내부 mutex와 version 갱신이 있다.
- **External Tool Usability:** 동일 T/ABI를 아는 client는 Open 후 Get/Set 가능하다. 이름만 아는 Tool에는 부족하다.
- **Gap:** type 변환/범위 검증, read-only enforcement, 실패 시 효과와 재시도 의미가 필요하다. 기존 Set은 commit 후 notify 오류를 반환할 수도 있으므로 실패를 무조건 미변경으로 해석하면 안 된다.
- **Recommended Minimal Extension:** R5 dynamic parameter access, 지원 primitive/string 범위와 write 결과 계약.
- **Priority:** P2 / Required.

### 11. Runtime status — PARTIAL

- **Requirement:** application 및 element 상태, loop heartbeat, runtime_error.
- **Current KCF Support:** S5 RuntimeStatusResponse의 PID/state/heartbeat/error, S8 Supervisor 캐시, S7 public aggregate 상태가 있다. heartbeat는 성공한 Loop 후 증가한다(process_runtime.cpp 104–111).
- **External Tool Usability:** aggregate SystemStatus는 읽을 수 있다. element별 상세는 owning Supervisor 채널 또는 로컬 runtime 객체에 한정된다.
- **Gap:** public header에 packet struct가 있어도 external client endpoint는 아니다. standalone은 supervision worker/socket을 만들지 않는다. aggregate 오류는 전체 element 상태 목록이 아니다.
- **Recommended Minimal Extension:** R1의 독립된 외부 status snapshot 및 stale/unknown 의미. 기존 supervision socket 공유 금지.
- **Priority:** P1 / Required.

### 12. Process identity — PARTIAL

- **Requirement:** PID, executable/process name, ExecutionMode, ProcessState와 element/application 관계.
- **Current KCF Support:** S9 PID, S8 spec의 executable와 friendly name, S5 state가 존재한다. S6 mode enum과 launch 환경 기반 detector가 있다.
- **External Tool Usability:** 정보가 여러 로컬 객체에 흩어져 있다. RuntimeStatusResponse에는 mode/executable/friendly name/application identity가 없다.
- **Gap:** DetectLaunchExecutionMode는 Run이 환경변수를 소비하기 전에 호출하는 launch intent 탐지이며 외부 조회가 아니다. PID는 이름 또는 실행 instance identity가 아니다.
- **Recommended Minimal Extension:** R1의 통합 process/element identity, standalone/supervised 구분과 PID 재사용에 대한 유효 범위.
- **Priority:** P1 / Required.

## Storage와 public contract 구분

### Topic

S1 private Storage는 magic, size=`sizeof(T)`, alignment=`alignof(T)`, format=2,
initialized, owner_pid, published_index, 32-bit publish_sequence, 세 Slot,
pthread notification 객체를 가진다. Slot도 T 정렬/크기에 따라 달라진다.
format=2는 고정된 현재 내부 형식이지 외부 안정 ABI 선언이 아니다.
Open은 `sizeof(Storage)`, payload size/alignment, magic/format/owner를 검사한다.
따라서 임의 byte 배열 T를 골라 Open하는 것은 semantic type 검증도 public generic API도 아니다.
ReadLatestSnapshot 역시 T가 필수이며 sequence는 Tool의 uint64와 달리 uint32이다.
향후 adapter는 wrap/재시작을 명시해야 하고 widening만으로 전역 단조 sequence가 되지 않는다.

특히 reader는 공유 Slot.users를 증가시켰다가 복사 후 감소시킨다. Publish는 다른
slot을 확보하지 못하면 -EAGAIN이다. reader crash가 pin 구간에 발생하면 감소를
완료하지 못할 수 있다. 그러므로 기존 read path를 그대로 generic화하는 것만으로
Tool crash 무영향 조건이 충족된다고 주장할 수 없다.

### Parameter

S3 private Storage는 owner_pid, payload_size/alignment, format=2, mutex/condition,
uint64 version/previous_version, active/previous slot, transaction flag 및 두 값 slot을 가진다.
owner는 Create/Unlink 수명 관리 주체이다. Open은 O_RDWR이며 Set에는 owner-only 또는
per-parameter writable 검사가 없다. OS 접근권한과 의미상 read-only는 별개이다.
Get/Set은 T와 동일 ABI가 필요하고 mutex를 사용한다. typed string 지원도 pointer/heap
없는 T라는 제약을 받으므로 임의 std::string 공유가 가능하다고 해석하지 않는다.
Tool이 private Storage를 reinterpret하거나 잠금/transaction 형식을 복제하는 방안은 제외한다.

### Runtime / SystemStatus

S9 Process가 socketpair를 생성하고 child에 FD를 넘긴다. S5는 inherited FD를 소비하고
Supervisor 요청 손실을 runtime 오류/정지로 연결한다. 공개 경로에 header가 있다는 것과
외부 Tool용 공개 접속 계약은 다르다. Tool이 이 FD를 공유하거나 요청을 끼워 넣으면 안 된다.
S7 SystemStatusSubscriber는 이미 별도 public read API지만 고정 aggregate status이며
application discovery, 전체 process identity 또는 element별 heartbeat를 제공하지 않는다.

## Type metadata 단계

| Level | 의미 | 현재 지원 | 첫 generic Data Viewer 필요성 |
| --- | --- | --- | --- |
| 0 | payload size/alignment | private storage 검증에 존재 | byte 수만으로 필드 표시 불가 |
| 1 | stable type ID / type name | 조사 범위에서 없음 | endpoint와 descriptor 대응에 필요 |
| 2 | field name, primitive kind, offset, size | 없음 | 최소 필요. x_m/y_m/yaw_rad 같은 의미를 크기로 추론할 수 없음 |
| 3 | nested/array/enum 등 | 없음 | Deferred |

첫 Viewer에는 Level 2에 해당하는 **의미 정보**가 필요하다. Framework API가 필드 값을
이미 변환해 반환하면 Tool 자체가 offset을 읽을 필요는 없다. descriptor와 raw snapshot을
연결하는 쪽에서는 bounds, primitive 폭/표현, string 길이/인코딩과 schema 일치 검증이 필요하다.
작은 flat primitive 및 bounded string 범위로 시작하며 재귀 reflection/IDL은 제안하지 않는다.

## Generic access 후보 비교와 추천

| 후보 | Coupling / ABI | 유지보수 / 안전성 | 적합성 |
| --- | --- | --- | --- |
| A. Raw SHM introspection | Tool이 private Storage, pthread/atomic/T ABI에 직접 결합 | layout 변경과 동기화/복구 규칙을 Tool이 복제. 잘못된 parse, pin/lock 손상 위험 | 배제 |
| B. KCF public introspection client API | Tool은 versioned public metadata/value 계약에만 결합. private ABI는 KCF가 관리 | thin client로 중앙 서비스 불필요. 단순 existing reader wrapper는 crash 격리를 해결하지 못하므로 observer-safe 계약이 필수 | **추천** |
| C. Dedicated local Tool protocol | Tool은 local protocol에 결합, KCF 내부 ABI와 분리 | Qt client crash와 서버를 분리하기 좋으나 endpoint lifecycle, 요청 제한, protocol 버전과 서버 유지 비용 추가. 서버 자체의 기존 data read도 안전성 검증 필요 | 필요 시 대안, 현재 필수 service 추가는 보류 |

**B를 추천한다.** Linux local/process 기반이며 Tool optional인 현재 범위에서 별도
mandatory daemon 없이 가장 작은 공개 client 경계를 만들 수 있다. 이것은 API 형태 선택이지
현재 SHM layout을 public으로 선언하자는 제안이 아니다. 구현 매체와 알고리즘은 결정하지 않는다.

B의 실제 채택에는 다음 조건을 충족하는 R4/R5 설계가 선행되어야 한다. 충족할 수 없다면
C의 별도 optional endpoint를 재평가한다. public API라는 이름만으로 격리가 보장되지는 않는다.

- Tool 미실행 시 KCF 정상 동작, Tool을 기다리는 readiness/heartbeat 조건 없음.
- Tool 종료/crash 시 control publisher가 pin/lock/ack 반환을 기다리지 않음.
- introspection 오류는 supervision 상태 전이·control callback을 발생시키지 않음.
- Publish/Subscriber 정상 경로에 descriptor 변환·동적 할당·Tool 응답 대기 추가 없음.
- 관측 요청 비용은 제한되어야 하며 추가 복사/동기화 비용을 0이라고 가정하지 않음.
- KCF에 Qt 의존성 없음. Tool에 application-specific header 및 private layout 의존성 없음.
- metadata 등록은 수명 변화 시 수행하는 방향이며 per-publish 등록은 불필요.
- Parameter write는 의도한 설정 변경이다. crash로 인한 불완전 변경/무기한 잠금과 정상 write 효과를 구분함.

## ToolBackend mapping

현재 [ToolBackend](../include/kcf_tool/backend/tool_backend.hpp)를 기준으로 모든 operation을 매핑한다.
R 번호는 [최소 Framework 확장](KCF_REQUIRED_FRAMEWORK_EXTENSIONS.md)을 참조한다.

| ToolBackend API | 필요한 KCF capability | GUI 변경 필요 여부 |
| --- | --- | --- |
| GetConnectionState() const | R1 접근 가능성/instance 유효성, 접근 오류 구분 | 기존 표시 유지 |
| Refresh() | R1/R2 목록·상태 refresh와 bounded 실패 처리 | bounded 호출이면 없음; blocking이면 후속 worker/queued 갱신 필요 |
| GetApplicationInfo() | R1 application identity + status | 현재 단일 application이면 없음; 다중 선택은 Deferred |
| GetElements() | R1 element/runtime/identity 목록 | 필드 충족 시 없음 |
| GetTopics() | R2 topic/role 목록 + R3 type metadata | 없음; unknown frequency를 0으로 오표시하지 않는 model/UI 정책은 후속 필요 |
| GetTopicInfo(name, out) | R2 lookup + R3 metadata | 기존 상세에 adapter 연결 가능 |
| ReadTopicLatest(name, out) | R4 snapshot + R3 flat type/value | API는 유지 가능; 호출/표시할 Viewer는 후속 추가 |
| StartTopicMonitor(name, callback) | R6 dynamic observation + R3/R4 | 향후 수명 안전한 queued delivery 필요, 현재 Echo disabled 유지 |
| StopTopicMonitor(name) | R6 cancel/in-flight/lifetime 계약 | Viewer 종료 시 cleanup은 후속 추가 |
| GetParameters() | R2 parameter 목록 + R3/R5 metadata/current values | 기존 필드 충족 시 없음 |
| GetParameter(name, out) | R5 dynamic read + R3 type/writable | 없음 |
| SetParameter(name, value) | R5 dynamic validated write | 기존 Apply/status 가능; blocking이면 비동기화 후속 필요 |

목표는 기존 탭과 ToolBackend를 유지하는 것이다. 다만 모든 future capability가 무조건
GUI 변경 없이 들어간다고 보장하지 않는다. 목록 getter에는 오류 채널이 없으므로 adapter는
Refresh 결과와 일관된 cache를 제공해야 한다. type 부족, 관계 미상, timestamp/frequency
미제공을 실제 값으로 꾸미지 않도록 optional/unknown 표현도 후속 계약에서 확정해야 한다.

## 구현 순서

1. **Phase 1 — Discovery:** R1 application/element/runtime/identity, R2 topic/parameter/role.
   R3의 최소 type identity 계약도 먼저 정한다. 먼저 이름/상태만 표시하며 unknown metadata를 숨기지 않는다.
2. **Phase 2 — Parameter Get/Set:** R3 primitive/string 범위, R5 value/writable/검증과 bounded write 결과.
3. **Phase 3 — Topic Latest:** R3 Level 2, R4 snapshot 및 observer crash 격리 검증.
4. **Phase 4 — Continuous Monitor:** R6 delivery/cancel/lifetime, queued GUI handoff.
5. **Phase 5 — Field Data Viewer:** 앞선 계약을 활용한 평면 필드 표시. Graph는 별도 Deferred 범위로 남긴다.

ROS graph clone, DDS, central master, network/remote discovery, schema language/IDL,
protobuf/JSON IPC, generic serialization bus, plugin/database, mandatory daemon은 제안하지 않는다.

## KT-2 검증

문서 외 Tool 코드 변경 없음. KcfBackend는 DISCONNECTED / -ENOTSUP 유지.
Tool의 기존 build 폴더를 지우고 Qt 6.9.3 경로를 지정하여 clean build PASS,
기존 `kcf_tool_backend_contract`, `kcf_tool_smoke` 2/2 PASS를 확인했다.
backend contract test에서 KcfBackend DISCONNECTED / -ENOTSUP 및 Qt-independent model을 검증했다.
문서의 12개 필수 record와 local link도 확인했다.
KCF는 읽기만 했으며 기존 working tree 변경을 유지한다. 실제 KCF 접속/값 설정은 수행하지 않는다.
