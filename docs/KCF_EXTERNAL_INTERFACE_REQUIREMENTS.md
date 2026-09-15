# KCF External Interface Requirements — KT-1

이 문서는 외부 Tool이 향후 KSS Control Framework에 요구하는 정보와 동작을
정의한다. KT-1은 Tool-side 계약 정의 단계이며 Framework 지원 또는 실제 연결을
의미하지 않는다. Framework 저장소를 변경하지 않는다.

- **Required**: 향후 실제 연동에 필요한 기본 기능. KT-1 구현 완료를 뜻하지 않는다.
- **Optional**: 제공되면 표시하되, 미제공 상태를 구분할 수 있어야 한다.
- **Deferred**: 후속 단계에서 요구사항과 의미를 합의한다.

## 요구 기능

| 항목 | 구분 | Tool 관점의 요구사항 |
| --- | --- | --- |
| 1. Application discovery | Required | 접근 가능한 application의 이름과 실행 상태, 발견 실패 또는 연결 불가 여부를 알 수 있어야 한다. 현재 Tool은 단일 application을 표시한다. |
| 2. Element discovery | Required | 선택 application에 속한 element를 열거하고 이름으로 구분할 수 있어야 한다. 목록 변경과 조회 실패를 구분할 수 있어야 한다. |
| 3. Topic discovery | Required | topic 이름과 type 이름, payload 크기를 열거하고 특정 topic의 metadata를 조회할 수 있어야 한다. |
| 4. Publisher/subscriber relation | Required | topic별 publisher/subscriber의 element 식별 정보를 얻고, 관계가 없다는 사실과 정보가 없다는 사실을 구분할 수 있어야 한다. |
| 5. Topic type metadata | Required | type 이름과 primitive/string 필드의 이름·type·표시용 값을 연결할 수 있는 metadata가 필요하다. 중첩 type, 배열 및 schema 진화는 Deferred이다. |
| 6. Topic latest data read | Required | 이름으로 최신의 일관된 sample을 읽고 type, sequence, 필드 값과 함께 제공할 수 있어야 한다. 데이터 없음, topic 없음, 미지원, 접근 실패를 구분해야 한다. |
| 7. Topic continuous observation | Required | topic 관측을 시작·중지하고 새로운 sample을 전달받을 수 있어야 한다. GUI thread에서 전달된다고 가정하지 않는다. 전달 thread, 순서, 유실, backpressure, 중지 및 수명 정책의 구체적 합의는 Deferred이다. |
| 8. Parameter discovery | Required | application에 속한 parameter를 이름으로 열거할 수 있어야 한다. |
| 9. Parameter metadata | Required | 각 parameter의 name, type, 현재 value, writable 여부를 얻을 수 있어야 한다. Tool은 현재 문자열로 값을 보관한다. |
| 10. Parameter get/set | Required | 이름으로 현재 값을 읽고 writable 값의 변경 결과를 확인할 수 있어야 한다. 미존재, 권한 거부, 잘못된 값, 미지원 및 연결 실패를 구분해야 한다. |
| 11. Runtime status | Required | application/element 실행 상태와 element heartbeat, runtime error를 조회할 수 있어야 한다. 상태와 오류의 의미 및 heartbeat의 유효성 판단 기준을 설명할 수 있어야 한다. |
| 12. Process identity | Required | element와 PID, executable, 실행 mode 사이의 대응을 얻을 수 있어야 한다. PID 재사용 시 이전 process와 혼동되지 않도록 식별 정보의 유효 범위를 설명해야 한다. |

## 추가 정보와 후속 범위

| 정보/기능 | 구분 | 요구사항 |
| --- | --- | --- |
| Topic frequency | Optional | 관측 주기 또는 frequency를 제공하면 단위와 측정 의미를 설명해야 한다. |
| Sample timestamp | Optional | 제공 시 microsecond 단위로 표현할 수 있고 clock 기준과 유효성을 알 수 있어야 한다. 미제공 시 현재 Tool snapshot에서는 0을 사용한다. |
| Process instance identifier | Optional | PID 재사용 구분을 돕는 instance 식별자나 시작 시각을 제공할 수 있다. 현재 model 확장은 Deferred이다. |
| 다중 application 선택 | Deferred | 현재 단일 application UI 이후에 선택·범위 계약을 정한다. |
| 중첩 구조·배열·재귀 reflection | Deferred | 현재 평면 field 목록으로 다룰 수 없는 type 표현은 후속 설계한다. |
| Launch/control, config, graph, logging | Deferred | KT-1 backend interface 범위에 포함하지 않는다. |

이 요구사항은 전달 경로, 저장 매체, discovery 구현, 프로세스 배치 또는 wire format을
규정하지 않는다. 구현을 선택하기 전에 외부 consumer가 위 의미를 얻을 수 있는지
Framework 측 public contract와 대조해야 한다.

## 현재 Tool-side API 의미

`MainWindow → ToolBackend ← MockBackend / KcfBackend` 구조를 유지한다.
Model과 callback type은 Qt 및 KCF header에 의존하지 않는다.

- `GetConnectionState() const`: backend 연결 상태. application의 실행 상태와 별개이다.
- `Refresh()`: 0 또는 negative errno. 실패 시 GUI는 이전 조회 결과를 지운다.
- 기존 application/list getter는 값 반환 API를 유지한다. 연결 상태와 Refresh 결과를
  함께 확인해야 한다. 빈 목록만으로 연결 성공이나 discovery 성공을 추론하지 않는다.
- `GetTopicInfo(name, out)`, `GetParameter(name, out)`, `ReadTopicLatest(name, out)`:
  성공 시 output을 채우며 실패 시 output을 변경하지 않는다.
- `SetParameter(name, value)`: 문자열 값을 설정한다. 실제 backend의 type 변환과
  검증은 향후 Framework type 계약에 맞춰 설계한다.
- `StartTopicMonitor(name, callback)`, `StopTopicMonitor(name)`: callback 기반 관측 계약.
  현재 두 backend는 항상 `-ENOTSUP`을 반환하며 callback을 저장하거나 호출하지 않는다.
  등록 중복, 빈 callback, callback 예외, stop 시 진행 중 callback, destructor 동기화의
  정책은 실제 monitor 구현 전에 확정해야 한다. 현재 계약은 thread 안전성을 보장하지 않는다.
- `DataSnapshot`: type 이름, sequence, timestamp_us, 평면 `FieldValue` 목록이다.
  sequence의 초기값·재시작·wrap 의미와 timestamp clock 기준은 실제 backend 계약에서
  설명해야 한다. `ValueKind`는 향후 분류용 vocabulary이며 descriptor 형식을 강제하지 않는다.
- Callback 인자의 참조는 호출 중에만 유효하다. 보관/전달 시 복사해야 한다.
  향후 GUI consumer는 수명이 보장되는 receiver를 대상으로 Qt queued invocation을
  사용해야 하며 callback에서 직접 widget을 수정하면 안 된다.

## KT-1 backend 지원 현황

| API | MockBackend | KcfBackend skeleton |
| --- | --- | --- |
| Connection | CONNECTED | DISCONNECTED |
| Application/list getter | 기존 mecanum mock 데이터 | 이름 없는 DISCONNECTED application / 빈 목록 |
| Refresh | 0, RUNNING heartbeat 증가 | -ENOTSUP |
| GetTopicInfo / GetParameter | 0 또는 -ENOENT | -ENOTSUP |
| SetParameter | 0 / -EPERM / -ENOENT | -ENOTSUP |
| ReadTopicLatest | -ENOTSUP | -ENOTSUP |
| Start/StopTopicMonitor | -ENOTSUP, callback 보관·호출 없음 | -ENOTSUP, callback 보관·호출 없음 |

실제 KCF 데이터 조회/변경, IPC 및 연결은 구현하지 않았다. KcfBackend는 mock 데이터로
연결된 것처럼 보이게 하지 않는다. Echo는 비활성화 상태를 유지한다.
