# Application / Element / Topic Graph

기존 ToolBackend discovery 계약을 사용하는 Local 구조 시각화다. Local Graph 자체는
Framework 및 기존 Local Backend 동작을 변경하지 않는다. 별도의 Network Backend 확장은
아래 Remote metadata 절에서 설명한다. 기존 여섯 탭 뒤에 Graph 탭을 추가하여 탭 순서와 기존
Elements/Topics/Parameters/Services 동작을 유지한다. 이 기능은 [v0.12](V0_12_RELEASE_NOTES.md)에 포함한다.

## 모델과 표현

`BuildGraph`는 GetApplications/GetElements/GetTopics/GetParameters/QueryServices로
얻어 MainWindow에 보관한 스냅샷을 입력받는다. 모델과 렌더링은 별도 파일에 있다.

| 대상 | 키 / 관계 |
| --- | --- |
| Element | Runtime PID + process_start_ticks, 사각형 노드 |
| Application | Supervisor PID + process_start_ticks, Element 그룹 |
| Standalone | Supervisor identity가 없는 Element의 별도 그룹 |
| Topic | logical name별 작은 노드, 각 registration을 보존하여 Publisher Element → Topic → Subscriber Element |
| Parameter | logical name별 노드, 실제 OWNER Element → Parameter → 실제 CLIENT Element |
| Service | Runtime + registration별 독립 server 노드, 소유 Element → server |

Element 소유 관계는 endpoint.identity.runtime으로만 연결한다. 같은 Element 이름,
TopicInfo.publishers/subscribers의 표시용 문자열, application 이름으로 추정하지 않는다.
같은 이름의 Application instance는 합치지 않는다. 스냅샷에 대응 Runtime이 없으면
endpoint는 All 보기에서 연결선 없이 표시하며 존재하지 않는 Element를 만들지 않는다.
같은 Runtime의 별도 registration도 보존하고 완전히 동일한 registration/role 중복만 제거한다.

노드와 edge는 identity/이름/역할 순으로 정렬한다. 왼쪽 Application 그룹의 Element와
오른쪽 endpoint를 두 열에 배치하는 deterministic layered layout이며 Subscriber/CLIENT
화살표는 오른쪽에서 왼쪽으로 향한다. Service 관계선은 호출 흐름이 아니라 server 소유를 뜻한다.
복잡한 교차선 최적화, force-directed layout, Graphviz 의존성은 없다.

## 조작과 기존 기능 연결

- Refresh Graph: 기존 MainWindow Refresh를 실행해 모든 탭을 같은 스냅샷으로 갱신한다.
  기존 계약에 따라 Echo를 중지하고, 미저장 Parameter 편집 또는 Service 호출 중에는 차단한다.
- Topics 기본 표시, Parameters/Services 기본 숨김. toggle/filter는 저장된 스냅샷만 다시 그린다.
- Application filter: All, 각 Supervisor instance, Standalone. 선택한 그룹에 속한
  Element와 그 endpoint만 표시하며 다른 Application으로 나가는 관계는 필터 밖에서 숨긴다.
  Refresh 후 필터 identity가 사라지면 All로 돌아간다.
- Zoom: 휠/+/-; Pan: 빈 배경 드래그; Fit: 전체 scene에 맞춤.
- Element/Topic 활성화: 더블클릭 또는 선택 후 Enter. Element는 정확한 Runtime으로
  Elements 탭을 연다. Topic이 여러 endpoint를 가지면 role/PID/start_ticks/registration
  메뉴에서 선택한 항목을 Topics 탭에서 연다. 사라진 identity는 선택하지 않는다.
- 미저장 Parameter 편집과 진행 중인 Service Call 보호를 탐색에도 적용한다.

## 검증

2026-09-26 검증:

- 실제 KCF + Qt GUI 구성: 전체 **16/16 PASS**. 현재 외부 Framework HEAD는
  `d672f23abdbace7a4fd41e1804eb69d2d48d95eb`; 외부 Framework 작업 파일을 수정하지 않았다.
  사용자 프로세스/SHM과 분리한 PID/SHM namespace에서 실행했다.
- KCF 없는 Mock/stub GUI 구성: **5/5 PASS**.
- `graph_model`: 1→1, 1→N, N→1, N→N; 같은 이름의 여러 Runtime; PID 재사용 세대 구분;
  Standalone/Supervised 혼합; endpoint 없는 Element; 정렬 결정성; 중복 registration;
  Parameter OWNER/CLIENT 및 owner 미발견; 개별 Service SERVER-only.
- `graph_smoke`: toggle/filter, Refresh endpoint 추가·제거, 실패 시 scene 비우기,
  동일 스냅샷의 레이아웃 유지, zoom, navigation/menu, 오래된 메뉴 선택 차단,
  MockBackend, 미저장 Parameter 편집 보호.
- 기존 `real_smoke`: 실제 KcfBackend로 Graph의 Runtime 연결과 종료 후 제거를 확인하며
  기존 Depth=1/4 Echo·재생성 검증을 유지한다.
- offscreen 이미지로 노드/그룹/방향 화살표 배치를 검토했다. 사용자의 실제 데스크톱
  수동 조작 검증과는 구분한다.

빌드/로그:

```sh
cmake -S . -B /tmp/kcf-tools-graph \
  -DKCF_SOURCE_DIR=/home/kssvm/workspace/kcf/kss_control_framework \
  -DKCF_TOOL_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_PREFIX_PATH=/opt/Qt/6.9.3/gcc_64
cmake --build /tmp/kcf-tools-graph -j4
ctest --test-dir /tmp/kcf-tools-graph --output-on-failure
```

기존 discovery 테스트는 다른 Runtime이 없는 환경을 전제로 한다. 사용자 Application을
중단할 수 없으면 별도의 PID/SHM namespace에서 실행한다. Graph 자체의 smoke/model
테스트는 실제 IPC를 만들지 않는다. 로그는 각 build의 `Testing/Temporary/LastTest.log`에 있다.

## 현재 표현할 수 없는 관계

Service client 및 미발견 Parameter client, 실제 packet/호출 흐름,
전송 성공 여부·주기·throughput은 추정하지 않는다. 같은 이름의 endpoint가 연결되어
보이더라도 ABI/type 호환 또는 실제 통신 성공을 보증하지 않는다. endpoint의 세부 타입과
오류 확인은 기존 Topics/Parameters/Services 화면을 사용한다.

MockBackend도 동일 모델을 사용한다. Mock의 표시용 publisher/subscriber 이름과
identity가 다를 경우에도 identity만 따르며 임의의 추가 Element를 만들지 않는다.

## Remote metadata 확장 (v0.12)

[Network 관측](NETWORK_OBSERVATION.md)의 Host 계층과 read-only Remote Detail을 추가했다.
위 2026-09-26 결과는 Local Graph의 기존 검증 기록으로 유지한다. 새 검증 결과와
외부 Gateway route/statistics 조회 API의 한계는 해당 문서에서 별도로 기록한다.
Remote 점선은 발견된 endpoint의 소유/등록 관계이며 실제 전송이나 호출의 증거가 아니다.
