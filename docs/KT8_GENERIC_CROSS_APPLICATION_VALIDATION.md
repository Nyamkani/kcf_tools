# KT-8 Generic Cross-Application Validation Result

검증일: 2026-09-15. 신규 검증 프로젝트는 `/tmp/kcf_generic_validation`에
작성했다. Tool 및 Framework 바깥의 독립 프로젝트이며, KCF를 외부 의존성으로
빌드한다. 기존 Tool의 MainWindow/KcfBackend/codec을 수정 없이 사용하고, GUI
테스트 번역 단위에는 Application 타입 헤더를 포함하지 않는다.

| 항목 | 결과 / 확인 내용 |
| --- | --- |
| Build | PASS — `/tmp/kt8-build`, C++17, Qt 6.9.3, 외부 KCF 링크 |
| Validation Applications | Alpha `sensor_suite`(sensor/processor), Beta `control_suite`(controller/actuator), Standalone `diagnostic_node` |
| Tool Source Changes | NONE — KT-8 시작 시점 대비 `include/`, `src/`, `CMakeLists.txt` SHA-256 및 git diff 동일 |
| KCF Core Changes | NONE — 외부 checkout의 `kcf/`, `bringup/` 파일 SHA-256 동일 |
| Application Discovery | PASS — 두 Application과 Standalone 동시 실행, 관리 Element 4개 + 독립 Runtime 1개 자동 표시 |
| Same-name Application Instances | PASS — 두 번째 `control_suite`를 추가하고 Supervisor PID/start_ticks로 별도 그룹 확인 |
| Membership | PASS — Runtime의 Supervisor PID/start_ticks와 Application 그룹 identity 일치 |
| Alpha Topic Echo | PASS — nested header의 sequence/timestamp_us, temperature, values[3], int16 quality; polling 후 sequence 증가 |
| Beta Topic Echo | PASS — sequence, command, feedback, uint8 mode, bool active 자동 표시 |
| Standalone Topic Echo | PASS — sequence, float64 load, bool healthy 표시 |
| Alpha Parameter | PASS — enabled/period_ms/scale GUI Apply, typed owner Get 및 GUI readback 일치 |
| Beta Parameter | PASS — kp/ki 및 limits[2] 수정, typed owner Get 및 GUI readback 일치 |
| Standalone Parameter | PASS — float64 threshold 수정/재조회 |
| Watcher | PASS — Alpha/Beta typed Parameter callback 변경 감지 및 증거 파일 기록 |
| Alpha Service | PASS — clear_count 입력 폼, accepted/reset_count 응답; 초기화 결과 0 |
| Beta Service | PASS — mode/target 입력 폼, accepted/error_code/applied_target 응답; target 12.5 반환 |
| Type Diversity | PASS — bool, int16, int32, uint8, uint32, uint64, float32, float64, fixed array |
| Application Isolation | PASS — Parameter 변경 상호 독립, Service 호출 전후 Topic/Parameter identity/type metadata 유지; Alpha ERROR/SAFE 때 Beta RUNNING 및 Echo/Get/Call 정상 |
| Reset Generation | PASS — Alpha Supervisor identity 유지, 두 child PID 교체, 이전 Element 선택 해제, 새 Echo/Get/Set/Call 정상 |
| Endpoint Re-registration | PASS — 동일 sensor Runtime에서 Topic Close/Unlink/Create, 캐시 identity는 ESTALE, Refresh 후 새 registration_id로 접근 |
| Application Exit / Restart | PASS — Alpha와 child만 제거, Beta/Standalone 유지; Alpha 재시작은 새 Supervisor identity, 이전 선택 재사용 없음 |
| Failure Injection | PASS — Publisher/Parameter owner 및 Service server 종료 후 기존 선택 접근 오류 처리, GUI crash 없음 |
| Existing Tool Regression | PASS — 10/10 |
| KCF-less | PASS — GUI 3/3, backend 1/1 |
| Framework Regression | PASS — 기존 runner 19개 항목 및 application scope/runtime supervision 추가 2개 실행 모두 성공 |
| mecanum | NOT MODIFIED |
| Result | PASS — 아래 명시한 검증 범위 및 순차 회귀 실행 기준 |

## 재현

```sh
cmake -S /tmp/kcf_generic_validation -B /tmp/kt8-build \
  -DCMAKE_PREFIX_PATH=/opt/Qt/6.9.3/gcc_64 \
  -DKCF_SOURCE_DIR=/tmp/kcf-r1-introspection \
  -DKCF_TOOLS_SOURCE_DIR=/home/kssvm/workspace/kcf/kcf_tools
cmake --build /tmp/kt8-build -j4
ctest --test-dir /tmp/kt8-build --output-on-failure
```

로컬 SHM/UDP 및 프로세스 실행 권한이 필요하다. Qt offscreen에서 실제 GUI의
목록 선택, Echo 버튼, Parameter 입력/Apply, Service 입력/Call을 구동한다.
시각적 레이아웃에 대한 수동 검토나 하드웨어 동작 검증은 포함하지 않는다.

성공한 KT-8 실행: 1/1 PASS, 3.00초.

* 로그: `/tmp/kt8-build/Testing/Temporary/LastTest.log`
* typed read/watch/status 증거: `/tmp/kt8-evidence-po27na`
* 독립 프로젝트 사용법: `/tmp/kcf_generic_validation/README.md`
* Tool 무변경 기준: `/tmp/kt8-tool-before.sha256`, `/tmp/kt8-tool-before.diff`
* Tool 확인 결과: `/tmp/kt8-tool-integrity.log`, `/tmp/kt8-tool-after.diff`
* Core 무변경 기준/결과: `/tmp/kt8-core-before.sha256`, `/tmp/kt8-core-integrity.log`

기존 KT-7.1 작업으로 이미 수정되어 있던 파일은 그대로 보존했다. 여기서
NONE은 git HEAD 대비 clean이라는 뜻이 아니라 KT-8 시작 시점 대비 추가
production 변경이 없다는 뜻이다. Tool 저장소에는 이 결과 문서만 추가했다.

## 회귀 검증

```sh
ctest --test-dir /tmp/kt71-build --output-on-failure
ctest --test-dir /tmp/kt71-no-kcf-gui --output-on-failure
ctest --test-dir /tmp/kt71-no-kcf-backend --output-on-failure
python3 /tmp/r2b1-regressions.py
/tmp/kcf-r1-introspection/build-r2b1/examples/supervisor/kcf_application_scope_test
/tmp/kcf-r1-introspection/build-r2b1/examples/supervisor/kcf_runtime_supervision_test \
  /tmp/kcf-r1-introspection/build-r2b1/examples/supervisor/kcf_supervisor_runtime_test
```

Framework 대상: R1, R2A, R2B 및 named CLI, R3, R4, R4.1, R5,
lifecycle, runtime health, Supervisor loss, reset, SystemStatus recovery,
Topic/Parameter recovery와 incomplete storage recovery, legacy Service,
Integration, R2B.1 application scope, runtime supervision.

회귀 runner 최종 출력은 `FAILED: []`, 추가 검증은
`Application-scoped SystemStatus PASS` 및
`Runtime supervision protocol/STARTING/RUNNING/heartbeat/arguments/environment/10 FD lifecycles PASS`였다.
Framework runner의 개별 로그는
`/tmp/kcf-r1-introspection/build-r2b1/r2b1-validation/`에 있다.
Tool/KCF-less 결과는 각 build 디렉터리의
`Testing/Temporary/LastTest.log`에 저장되어 있다.

KT-8 내부에서는 여러 Application을 동시에 실행했다. 기존 회귀 fixture는
전역 discovery 개수를 전제로 하므로 KT-8 종료 후 순차 실행한다. 기존 회귀
전체를 KT-8 프로세스들과 동시에 실행하는 조건은 검증하지 않았다.

## Problems / 범위

* 테스트 초기 실행에서 startup snapshot의 Runtime/Endpoint 등록 시차와
  이전 선택의 준비 조건을 잘못 가정했다. 독립 테스트만 보완했다.
* 종료된 owner 접근은 Get=-116(ESTALE), Set=-2(ENOENT)였다. Runtime/type
  조회에서 먼저 종료를 감지할 수 있으며, 요청에서 허용한 오류 처리에 해당한다.
* Echo 오류 뒤에는 기존 GUI 계약대로 Refresh 후 다시 선택/시작한다.
* SAFE는 public application-scoped SystemStatus를 읽어 기록하는 fixture의
  모의 상태이며, 실제 하드웨어 출력 제어를 의미하지 않는다.
* 동일 이름 Application은 서로 다른 endpoint prefix를 사용한다. 같은
  logical Topic에 두 Publisher를 만드는 선택 항목은 global SHM ownership
  충돌을 피하기 위해 수행하지 않았다.
* Graph, Launch, plotting, history, auto watch, Dynamic Monitor, Service traffic
  monitor, Action, remote control, hardware 전용 기능은 구현하지 않았다.
* commit/push하지 않았다.
