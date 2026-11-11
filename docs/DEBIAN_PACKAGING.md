# Debian 패키징 1단계

## 2단계: Ubuntu별 패키징 환경과 산출물

Tools DEB 지원 조합은 **Ubuntu 24.04 amd64만**이다. Ubuntu 24.04/26.04 arm64 및
Ubuntu 26.04 amd64는 거부한다. Core의 ARM64 지원 예정과 Tools 지원 범위를 혼동하지 않는다.
ARM64 실제 빌드·검증은 미완료이며 가상 정책 검증을 ARM64 빌드 성공으로 기록하지 않는다.

패키징을 활성화할 때만 `/etc/os-release`의 `ID`/`VERSION_ID`를 읽고, 파일이 없을 때만
`/usr/lib/os-release`로 대체한다. `dpkg --print-architecture`, 대상 CPU/64-bit ABI를 검사하며
식별 실패·미지원 조합·교차 컴파일은 명시적 오류로 종료한다. 일반 개발/Mock 빌드는 이 제한을 적용하지 않는다.

Upstream version `0.14.0`은 유지하고 revision은 `1+ubuntu${VERSION_ID}`로 자동 생성한다.
GitHub Release 산출물 규칙은 `<package>_<upstream>-<revision>_<architecture>.deb`이며
현재 지원 산출물은 `kcf-tools_0.14.0-1+ubuntu24.04_amd64.deb`이다.
아래 1단계의 `-1` 파일명과 검증 기록은 당시 이력이다. Release 업로드는 별도 사용자 작업이다.

Qt6/glibc/libstdc++ 최소 버전은 `CPACK_DEBIAN_PACKAGE_SHLIBDEPS=ON`과 빌드 환경의
symbols/shlibs 정보를 사용해 자동 생성하며 버전을 수동 고정하지 않는다.
동적 로드되는 플랫폼 플러그인 `qt6-qpa-plugins`의 명시적 의존성은 유지한다.
정적으로 포함한 Core는 별도 Core DEB 의존성을 만들지 않으며 Core 갱신 시 Tools 재빌드가 필요하다.
최종 Depends/ELF 검사와 실제 Backend/Qt offscreen 검증은 자동 의존성 생성과 별도로 수행한다.

KCF Tools v0.14.0을 `kcf-tools` DEB로 제공한다. `/usr/bin/kcf_tool`만 설치하며,
실제 KCF Backend를 포함한다. 기본 실행 Backend는 기존과 같이 Mock이다.
실제 Backend는 `kcf_tool --backend kcf`로 선택한다.

## 빌드

배포 대상 Debian/Ubuntu의 Qt6 Widgets/Network 개발 패키지, QPA plugins,
nlohmann-json3-dev, C++17 compiler, CMake, dpkg-dev가 필요하다.
Qt 온라인 설치 프로그램의 `/opt/Qt` SDK를 배포판 Qt 패키지로 간주하지 않는다.
해당 SDK로 링크한 바이너리에 배포판 Qt 의존성을 임의로 붙이거나 시스템 라이브러리를 복사하지 않는다.

```sh
cmake -S . -B /tmp/kcf-tools-deb-build \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=/usr \
  -DBUILD_SHARED_LIBS=OFF \
  -DKCF_SOURCE_DIR=/path/to/kss_control_framework \
  -DKCF_TOOL_BUILD_GUI=ON -DKCF_TOOL_BUILD_TESTS=ON \
  -DKCF_TOOL_ENABLE_PACKAGING=ON
cmake --build /tmp/kcf-tools-deb-build -j2
cpack --config /tmp/kcf-tools-deb-build/CPackConfig.cmake -B /tmp/kcf-tools-debs
dpkg-deb --info /tmp/kcf-tools-debs/kcf-tools_0.14.0-1_amd64.deb
dpkg-deb --contents /tmp/kcf-tools-debs/kcf-tools_0.14.0-1_amd64.deb
dpkg-deb --extract /tmp/kcf-tools-debs/kcf-tools_0.14.0-1_amd64.deb /tmp/kcf-tools-extracted
QT_QPA_PLATFORM=offscreen /tmp/kcf-tools-extracted/usr/bin/kcf_tool --help
```

`amd64`는 빌드 환경의 예시이며 ARM64 빌드 성공을 의미하지 않는다.
패키징을 켜면 실제 Backend, GUI, 정적 Core 및 FrameworkVersion 5.6.0을 검사한다.
실제 Backend 전체 컴파일과 회귀 테스트로 API 연동을 추가 확인해야 한다.
패키징 OFF(기본값)에서는 기존 Mock/stub, GUI OFF 및 직접 소스 링크 빌드를 유지한다.

## 의존성과 제외 항목

Core는 `KCF_SOURCE_DIR/kcf`에서 정적으로 빌드되어 GUI에 포함된다. 따라서 `kcf-core` DEB는
Tools 실행 의존성이 아니다. Core 변경을 반영하려면 Tools를 다시 빌드·배포해야 한다.
Core SDK·Gateway·Bringup·예제·테스트를 Tools DEB에 중복 설치하지 않는다.

Qt6 및 libc/libstdc++ 등은 `dpkg-shlibdeps`가 실제 링크 결과와 배포판 메타데이터로 의존성을
생성한다. 동적 로드되는 플랫폼 플러그인은 `qt6-qpa-plugins` 의존성을 별도로 선언한다.
Qt 라이브러리/플러그인을 번들하지 않으며, Qt 설치 경로를 설치 바이너리의 RPATH에 넣지 않는다.
Debug 패키지는 개발 검증용이다. 배포판/아키텍처가 다르면 별도 빌드·검증한다.

테스트는 별도 Linux network/IPC/mount namespace와 private SHM, 임시 XDG 설정 디렉터리에서
직렬로 실행한다. 예: Core 저장소의 `tests/stabilization/run_regressions.py --mode smoke`.
Qt GUI는 offscreen으로 검사하고 기존 Host ID 및 사용자 설정은 건드리지 않는다.
시스템 패키지 설치·서비스 자동 시작·실제 하드웨어 시험은 이 단계의 범위가 아니다.

## 2026-11-11 개발 VM 검증

기준: `main` / `9eb0e6f` + 패키징 변경, Core `dev` / `570a233` + 패키징 변경.
Ubuntu 24.04 / amd64(x86_64), Debug, 실제 KCF 5.6.0 Backend로 빌드했다.
ARM64나 시스템 설치·운영 시험은 수행하지 않았다.

VM에는 배포판 Qt6 대신 `/opt/Qt/6.9.3` SDK만 있었다. 시스템 설치 없이 Ubuntu 24.04의
Qt6 6.4.2 개발·런타임 DEB 19개를 `/tmp/kcf-deb-phase1/qt-debs`에 다운로드하고
`qt-root`에 압축 해제했다. 패키지의 실제 control/symbols/shlibs/list를 사용하는 임시
`qt-dpkg` 조회 DB와 `bin/dpkg-shlibdeps` wrapper로 의존성을 계산했다.
시스템 dpkg DB나 Qt 파일은 변경하지 않았다. Overlay mount 시도는 환경 미지원으로 실패하여
`CMAKE_PREFIX_PATH`를 추출 경로로 지정했다. 이 임시 환경은 제품 설치 파일에 포함되지 않는다.

| 검증 | 결과 |
| --- | --- |
| 실제 Backend Debug configure/build, Core 5.6.0 compile-time 검사 | PASS |
| 기존 Tools CTest (각각 private namespace, Qt offscreen) | 22/22 PASS |
| Element name graph / 실제 Gateway Local IPC 별도 회귀 | 2/2 PASS |
| Core 없이 GUI OFF인 Mock/stub configure/build/CTest | PASS, 1/1 |
| CPack, dpkg-deb info/contents, 임시 압축 해제 | PASS |
| 추출 실행파일 `--help` | PASS, exit 0 |
| 추출 GUI `--backend kcf --no-network` offscreen | 3초 정상 유지 후 시험에서 SIGTERM, startup PASS |
| Mock 패키징 및 비패키지 `/opt/Qt` SDK 패키징 거부 | PASS (의도된 configure 실패) |

최종 DEB의 실행파일은 위 smoke 실행파일과 SHA-256이 동일함을 확인했다.
GUI startup 검증은 실제 화면/하드웨어/네트워크 기능의 신규 수동 검증을 뜻하지 않는다.

- `/tmp/kcf-deb-phase1/final/tools/kcf-tools_0.14.0-1_amd64.deb`
- SHA-256: `1893e6b56b33d3fd9262ae2a88498632dcdb08aa0eacf2ad39846a44e5e09e31`
- 파일 1개: `/usr/bin/kcf_tool`. Core/Qt/system 공유 라이브러리, 서비스/설정 파일, 테스트 없음.
- Depends: `qt6-qpa-plugins`, `libc6 (>= 2.38)`, `libgcc-s1 (>= 3.0)`,
  `libqt6core6t64 (>= 6.4.0)`, `libqt6gui6t64 (>= 6.4.0)`,
  `libqt6network6t64 (>= 6.1.2)`, `libqt6widgets6t64 (>= 6.3.0)`, `libstdc++6 (>= 12)`.
- `kcf-core` 패키지 의존성 및 RPATH/RUNPATH 없음.

로그: `/tmp/kcf-deb-phase1/tools-*.log`, `tools-final-info.txt`, `tools-final-contents.txt`,
`smoke/results.json`, `mock-ctest.log`, `packaged-tools-*.log`, `packaged-tools-smoke.json`,
`packaging-guards.json`, `final-artifacts.json`. `/tmp`는 영구 artifact 보관소가 아니다.
공동 Core 검증의 구형 `runtime_health` FAIL은 Core 패키징 가이드에 별도로 보존했으며,
Tools 테스트 PASS를 전체 과거 회귀 무실패로 확대하지 않는다.
시스템 설치·서비스 운영, 기존 Host ID/설정 변경, commit/push/tag/release는 수행하지 않았다.
