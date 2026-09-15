# KCF Tool — KT-7

Linux C++17 / Qt6 Widgets 기반 standalone Tool입니다. GUI는 `ToolBackend`만 사용하며,
기본 `MockBackend`와 선택적인 실제 `KcfBackend`를 제공합니다.

## 빌드 및 실행

Mock 구성은 KCF 없이 빌드할 수 있습니다.

```sh
cmake -S . -B build -DKCF_TOOL_BUILD_TESTS=ON
cmake --build build -j
./build/kcf_tool --backend mock
ctest --test-dir build --output-on-failure
```

실제 KCF 연결은 R1–R5 개발 소스를 외부 dependency로 지정합니다. 소스를 복사하지 않습니다.

```sh
cmake -S . -B build-kt3 \
  -DKCF_SOURCE_DIR=/tmp/kcf-r1-introspection \
  -DKCF_TOOL_BUILD_TESTS=ON \
  -DCMAKE_PREFIX_PATH=/opt/Qt/6.9.3/gcc_64
cmake --build build-kt3 -j
./build-kt3/kcf_tool --backend kcf
ctest --test-dir build-kt3 --output-on-failure
```

Qt가 기본 검색 경로에 있으면 `CMAKE_PREFIX_PATH`는 생략합니다.
`-DKCF_TOOL_BUILD_GUI=OFF`로 Qt 없는 backend 전용 빌드도 가능합니다.
`KCF_SOURCE_DIR`를 생략한 빌드의 `--backend kcf`는 기존 disconnected stub입니다.
실제 통합 테스트는 하드웨어 없이 테스트용 프로세스, SHM 및 loopback UDP를 사용합니다.
다른 KCF 프로세스가 없는 환경에서 실행하십시오.

## 동작

- Applications 트리는 Supervisor identity별 Application과 소속 Runtime을 표시합니다.
- membership에 연결되지 않은 Runtime은 Standalone에 표시합니다. 이름 대신 PID + start_ticks로 구분합니다.
- Element를 더블클릭하거나 Enter로 열고, 상세 endpoint를 같은 방법으로 열면 기존 Echo/Editor/Call 화면으로 이동합니다.
- 수동 Refresh로 application/runtime/endpoint/service 목록을 갱신합니다. 사라진 세대의 선택은 해제됩니다.
- 실행 중인 유효 runtime 또는 supervisor가 있으면 Connected, 없으면 Disconnected입니다.
- backend는 Topic latest 읽기, Parameter Get/Set, Service Call을 제공합니다.
- 기존 화면에 Services 탭을 추가했습니다. Parameters에서는 scalar·fixed array를 조회하고 필드별로 편집합니다.
- Apply는 입력 검증 후 최신 값을 읽어 수정 항목을 반영하고, 성공 뒤 실제 값을 다시 조회합니다.
- 편집 중에는 Apply 또는 Revert 전까지 선택 변경·Refresh·창 닫기를 막습니다. 자동 쓰기는 없습니다.
- Services에서 request를 편집하고 Call하면 worker가 요청하고 GUI에 response를 표시합니다.
- Call 중에는 backend 조작을 잠시 비활성화하고 Echo를 중지합니다. GUI event loop는 계속 동작합니다.
- Timeout은 callback 미실행을 보장하지 않으며 Tool은 자동 재호출하지 않습니다.
- Topics에서 publisher를 선택하고 Start Echo를 누르면 100 ms마다 scalar/array 값을 표시합니다.
- Echo 동안 하나의 reader를 유지하며, R4.1의 SHM 재생성 감지를 사용합니다.
- Stop Echo, Topic 변경, Refresh, 창 닫기 또는 치명적 읽기 오류 시 polling을 중지합니다.
- 재생성 감지 후 Refresh와 Start Echo가 필요합니다. 첫 sample 대기는 자동 중지하지 않습니다.
- Dynamic Monitor API는 계속 `-ENOTSUP`입니다. graph/launch 확장은 미구현입니다.
- Mock의 시연 데이터와 기존 독립 테스트는 유지됩니다.

Current KCF introspection development compatibility: **SHM format = 3, Service protocol = 2**.
실제 연결되는 모든 KCF 프로세스는 동일 Framework revision으로 **rebuild + restart**해야 합니다.

KT-7의 GUI/회귀 테스트는 통과했지만, 현재 외부 KCF Bringup은 전역 SystemStatus storage의
단일 writer 제약으로 두 Application을 동시에 시작할 수 없습니다(`-EEXIST`).
따라서 요청한 전체 KT-7 acceptance 결과는 **FAIL (다중 실제 Bringup 항목 미충족)**입니다.
[KT-7 구현·검증·제한 보고서](docs/KT7_APPLICATION_ELEMENT_EXPLORER.md)를 참고하십시오.

[KT-6 Service Call UI 완료 보고서](docs/KT6_SERVICE_CALL_UI.md),
[KT-5 Parameter Viewer / Editor 완료 보고서](docs/KT5_PARAMETER_VIEWER_EDITOR.md),
[KT-4.1 persistent Echo 완료 보고서](docs/KT4_1_PERSISTENT_TOPIC_ECHO.md)를 참고하십시오.
실제 Echo에는 Framework R4.1 적용 버전이 필요합니다.
[KT-4 당시 구현·제한](docs/KT4_TOPIC_ECHO_UI.md)의 recreate 제한은 KT-4.1에서 해결했습니다.
[KT-3 구현·API·검증 결과](docs/KT3_REAL_KCF_BACKEND.md)를 참고하십시오.
이전 설계 문서는 당시의 분석 기록입니다:
[KT-1 외부 계약](docs/KCF_EXTERNAL_INTERFACE_REQUIREMENTS.md),
[KT-2 Gap Analysis](docs/KCF_INTERFACE_GAP_ANALYSIS.md),
[Framework Extensions](docs/KCF_REQUIRED_FRAMEWORK_EXTENSIONS.md).
