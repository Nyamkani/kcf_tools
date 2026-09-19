# v5.1 수동 SHM 재연결 진단

이 진단은 [KCF Tool v0.11](V0_11_RELEASE_NOTES.md)의 Framework v5.1 연동 기록이다.
진단 이후 사용자가 실제 GUI의 값 갱신과 SHM 재연결을 수동 확인한 결과는
v0.11 릴리스 노트에 자동 테스트 14/14 PASS와 구분하여 반영했다.

**원인: rebuild 이전 GUI와 v5.1 GUI의 혼용. Framework/현재 Tool 재연결 구현 결함이 아니다.**
실제 오류 창을 PID와 연결하고, 구버전 실행 파일을 별도 offscreen 프로세스로 실행해
동일한 오류와 반환 지점을 재현했다. 사용자 GUI에 debugger를 attach하거나
사용자 Fixture/SHM을 변경하지 않았다.

## 확인된 사실

- Framework HEAD: `8360b6e053472ba20acdfc62b7489e33122997ec`, branch `dev`, 변경 없음.
- `build-kt3/CMakeCache.txt`의 `KCF_SOURCE_DIR`는
  `/home/kssvm/workspace/kcf/kss_control_framework`이다. build type은 빈 값이며,
  Fixture는 `-std=c++17 -UNDEBUG`, Core는 `-std=c++17`로 빌드됐다.
- Fixture PID 147234와 GUI PID 147550의 `/proc/PID/exe` 해시는 현재
  `build-kt3` 파일과 각각 일치한다.
- 별도로 GUI PID 450694가 2026-09-18부터 실행 중이다. 실행 파일은
  `build-kt3/kcf_tool (deleted)`이며, 현재 파일과 해시가 다르다.
  이 바이너리의 `DynamicStorage::Open`은 format **3**을 검사한다.
- 창 전용 픽셀 버퍼를 읽어 확인한 결과, PID 450694 창에
  `Echo unavailable: Protocol error`가 표시돼 있었다. PID 147550 창은
  endpoint 1에서 정상적인 `Topic was removed or recreated` 중지 상태였다.
  구버전 창에는 재생성된 endpoint 6이 선택되어 있었다.
- 호스트의 `/dev/shm/manual_v51%2Ftopic` inode=195372, size=312를 직접 확인했다.
  header는 magic=TOPIC_MAGIC, payload size=32, alignment=8, format=4,
  initialized=1, owner_pid=147234, depth=1이다.
- 해당 Topic에 현재 `build-kt3` 라이브러리로 만든 진단 프로그램을 연결했을 때
  Refresh, GetTopicInfo, GetType, DynamicTopicReader::Open, StartTopicEcho가 모두 0이었다.

현재 GUI 파일 SHA-256:
`f7fce64f462c4a0943e58f67cba0dbb8ce00420c3b93098bef5d75a391d0500f`

PID 450694 실행 파일 SHA-256:
`f6c7c42b3a90277e643c3c3f475ca1dc13c56a8dbbed990364803c79325f76f5`

초기 샌드박스 조회에서는 호스트 PID/SHM이 보이지 않았다. 위 확인은 호스트에서
별도로 수행한 읽기 전용 조회 결과이며, 해당 프로세스나 SHM을 삭제하지 않았다.

## 오류 경로 구분

`StartTopicEcho`는 registration 검증 → GetType → DynamicTopicReader::Open
→ registration 재검증 순서로 진행한다.

v5.1의 `kcf/src/dynamic_access.cpp`, `DynamicStorage::Open`은 다음 경우
`-EPROTO`를 반환한다: 헤더 읽기 길이 불일치, magic/format/owner_pid 불일치,
depth 읽기 실패, depth/layout 계산 실패, 실제 파일 크기와 계산된 길이 불일치.
이어지는 Validate에서도 헤더 또는 depth 변경을 검사한다.
타입 ID/layout 불일치는 `-EPROTOTYPE`, payload 크기/alignment 불일치는 `-EMSGSIZE`다.

관측한 Topic은 Header 168 + 3 slots × 48 = **312 bytes**로 크기 검사를 통과한다.
descriptor 조회와 실제 Open도 성공했으므로, 현재 이 SHM의 헤더/Depth/크기가
잘못됐다는 증거는 없다.

구버전 GUI를 복사해 offscreen으로 실행하고 같은 `/manual_v51/topic`을 선택했다.
GDB 반환값과 backtrace로 확인한 실제 실패 경로는 다음과 같다.

```text
KcfBackend::StartTopicEcho
  → KcfBackend::Impl::Type
  → IntrospectionClient::GetType
  → IntrospectionClient::ListTypes
  → SharedChannel<TypeRegistrySnapshot>::Open
  → FailOpen(-EPROTO)

TypeRegistry SHM actual size:  2789688
old binary expected size:     2789680
GetType return:               -71
GUI: Echo unavailable: Protocol error
```

구버전 `SharedChannel<TypeRegistrySnapshot>::Open+0x2bb`에서 파일 크기를
`0x2a9130`(2789680)과 비교하고 불일치하여 실패했다.
**descriptor 조회용 TypeRegistry storage의 구버전 layout 크기 검사 실패**이며,
application Topic의 `DynamicTopicReader::Open`까지 도달하지 않았다.
312-byte application Topic의 Depth/크기 실패나 descriptor 내용의 유효성 실패가 아니다.
참고로 구버전 DynamicStorage도 format 3을 요구하지만 이번 실제 반환 지점은 그보다 앞이다.

현재 `build-kt3/kcf_tool`을 동일한 offscreen 선택 절차로 실행하면
`Echo stopped; Start Echo enabled=1`로 정상 연결 준비가 된다.
즉 최초 Echo/USR1 확인은 새 창에서, Refresh/Publisher 재선택 확인은 구버전 창에서
관측된 상태와 일치한다. 자동 테스트는 같은 빌드의 새 프로세스만 실행하므로 통과했다.

## 재현 및 회귀

- 기존 `build-kt3` Fixture로 별도 이름의 Topic을 생성하고 USR1, WINCH,
  WINCH를 2초 간격으로 실행: 각 단계 GetType/Open/StartTopicEcho 모두 성공.
- 기존 `build-kt3`의 Backend/Echo smoke Depth=1/4: 격리 환경에서 **4/4 PASS**.
- 추가 테스트 `tests/topic_reconnect.cpp`: Depth=1/4 각각 reader를 유지한 채
  재생성 전후 1초 대기, 세 번 반복. 이전 reader는 ESTALE, Refresh 후 새로운
  registration으로 descriptor/Open/Echo 성공. 다른 수동 Runtime과 공존하도록
  테스트 고유 endpoint만 선택한다.
- 전체 회귀 **14/14 PASS** (20.10초). 결과는
  `/tmp/kcf-tools-v51-8360b6e/Testing/Temporary/LastTest.log`에 있다.
- 오류 자체의 비교 재현: 복사한 구버전 GUI는 GetType=-71 및 Protocol error,
  현재 GUI는 연결 준비 성공. 진단 기록은 `/tmp/kcf-old-trace.log`, 실행 파일은
  `/tmp/kcf-tool-old-450694`, offscreen 조작 helper는 `/tmp/kcf_gui_probe.so`이다.
  helper는 새 진단 프로세스에서 Refresh/Topic 선택만 수행한다. 이 임시 자료는
  해당 구버전 바이너리에 대한 원인 분석용이며 일반 CTest 의존성이 아니다.

기존 자동 테스트는 새 바이너리를 실행하고 외부 Runtime이 없는 환경을 전제로 한다.
수동 환경에는 GUI 두 버전과 사용자 Fixture가 공존했다. 실제 호스트에서 기존
테스트를 그대로 실행하면 Runtime 개수 전제로 실패했으나 재연결 오류는 아니었다.
사용자 프로세스를 중단하지 않고 별도 PID/SHM namespace에서 비교했다.

```sh
unshare --user --map-root-user --mount --pid --fork --mount-proc sh -c \
  'mount -t tmpfs tmpfs /dev/shm && ctest --test-dir /tmp/kcf-tools-v51-8360b6e --output-on-failure'
```

Framework/Backend/GUI 구현 수정은 필요하지 않다. 수정은 CMake의 회귀 테스트 등록,
신규 테스트, README의 Tool 재시작 안내 및 이 진단 문서로 한정했다.
기존 v5.1 작업 변경은 보존했다.

## 수동 재검증

1. 오래 실행 중인 GUI 창을 사용자가 종료하고, 검증된 바이너리로 새 창을 연다:
   `/tmp/kcf-tools-v51-8360b6e/kcf_tool --backend kcf`.
2. `/manual_v51/topic [PUBLISHER]` 선택 후 Echo를 시작한다.
3. Fixture PID가 여전히 147234인지 확인한 후 `kill -USR1 147234`,
   `kill -WINCH 147234`를 순서대로 실행한다.
4. 기존 Echo의 stale 중지를 확인하고 Refresh → Publisher 재선택 → Start Echo.
5. 오류가 지속되면 오류 창의 PID를 기록하고 해당 PID의 `/proc/PID/exe`와
   디스크 파일의 `sha256sum`을 비교한다. 새 GUI에서도 재현되어야 다음 단계로
   실제 실패 반환 지점을 추적할 수 있다.

사용자의 데스크톱에서 직접 수행하는 최종 재검증은 위 순서를 따른다.
기존 사용자 창은 자동으로 닫지 않았다. 오류 창 식별과 구버전/현재 버전 비교 재현은
완료했으며, 새로운 GUI로 다시 실행하면 구버전 Core 혼용을 제거할 수 있다.
commit/push 및 SHM 일괄 삭제는 수행하지 않았다.
