# Debian 패키징 — Tools v0.15.0 / Core 5.6.1

## 2단계: Ubuntu별 패키징 환경과 산출물

Tools DEB 지원 조합은 **Ubuntu 24.04 amd64만**이다. Ubuntu 24.04/26.04 arm64 및
Ubuntu 26.04 amd64는 거부한다. Core의 ARM64 지원 예정과 Tools 지원 범위를 혼동하지 않는다.
ARM64 실제 빌드·검증은 미완료이며 가상 정책 검증을 ARM64 빌드 성공으로 기록하지 않는다.

패키징을 활성화할 때만 `/etc/os-release`의 `ID`/`VERSION_ID`를 읽고, 파일이 없을 때만
`/usr/lib/os-release`로 대체한다. `dpkg --print-architecture`, 대상 CPU/64-bit ABI를 검사하며
식별 실패·미지원 조합·교차 컴파일은 명시적 오류로 종료한다. 일반 개발/Mock 빌드는 이 제한을 적용하지 않는다.

Upstream version은 `0.15.0`이며 revision은 `1+ubuntu${VERSION_ID}`로 자동 생성한다.
GitHub Release 산출물 규칙은 `<package>_<upstream>-<revision>_<architecture>.deb`이며
생성한 산출물 이름은 `kcf-tools_0.15.0-1+ubuntu24.04_amd64.deb`이다.
아래 Historical 절의 v0.14 `-1` 파일명과 검증 기록은 당시 이력이다. Release 업로드는 별도 사용자 작업이다.

Qt6/glibc/libstdc++ 최소 버전은 `CPACK_DEBIAN_PACKAGE_SHLIBDEPS=ON`과 빌드 환경의
symbols/shlibs 정보를 사용해 자동 생성하며 버전을 수동 고정하지 않는다.
동적 로드되는 플랫폼 플러그인 `qt6-qpa-plugins`의 명시적 의존성은 유지한다.
정적으로 포함한 Core는 별도 Core DEB 의존성을 만들지 않으며 Core 갱신 시 Tools 재빌드가 필요하다.
최종 Depends/ELF 검사와 실제 Backend/Qt offscreen 검증은 자동 의존성 생성과 별도로 수행한다.

KCF Tools v0.15.0을 `kcf-tools` DEB로 제공한다. `/usr/bin/kcf_tool`만 설치하며,
실제 KCF Backend를 포함한다. 기본 실행 Backend는 기존과 같이 Mock이다.
실제 Backend는 `kcf_tool --backend kcf`로 선택한다.

## v0.15 검증 상태

최종 Debug 개발 환경은 **Core v5.6.1 / Qt 6.9.3**, `build-main`이다.
PeerDiscovery 재컴파일 → 정적 Core → Tool 재링크 후 **CTest 22/22 PASS (92.44초)**,
별도 Mock/stub **6/6 PASS (4.08초)**를 확인했다. Virtual LAN / Qt offscreen E2E도 PASS이며
READY/STALE/INVALID, 선택·Subscribe Handle 유지, LOST/Session 변경, Verified Route 및 Graph를 검증했다.
최초 sandbox netlink 제한으로 환경 준비가 실패했고, 권한을 허용한 격리 namespace 실행은 PASS했다.
로그는 `/tmp/kcf-tools-015-final-alAyLy5z`에 있다.

Release 패키징은 **Ubuntu 24.04 amd64 / Release / 배포판 Qt 6.4.2+dfsg-21.1build5**로 수행했다.
입력 Core DEB는 `kcf-core_5.6.1-1+ubuntu24.04_amd64.deb`이며 SHA256은
`b057ef8c02e600aa563b69f7f1a5c5dc7c9168c89b6910b35ccbeef22b710fa5`다.
Core 저장소나 시스템 Core 5.6.0 대신 추출 SDK의 헤더·정적 라이브러리를 최종 빌드에 사용했다.

| 검증 | 최종 결과 |
| --- | --- |
| Core 5.6.1 버전·필수 Snapshot/Remote Client API 검사 | PASS |
| CMake Configure / Release Build / CPack | PASS (최초 staging 오류는 아래 기록) |
| DEB info/contents·추출 `--help` | PASS |
| 실제 KCF Backend | compile definitions·심볼·SDK 정적 링크 및 offscreen startup PASS |
| 추출 GUI `--backend kcf --no-network` | 4초 정상 유지 후 해당 시험 프로세스만 SIGTERM |
| Qt 런타임/Plugin | 시스템 배포판 라이브러리 및 `/usr/lib/x86_64-linux-gnu/qt6/plugins` 사용 |
| RPATH/RUNPATH | 없음; `/opt/Qt` 및 임시 빌드 경로 없음 |
| 파일·권한·Maintainer Script | `/usr/bin/kcf_tool`만 포함, root:root 0755, Maintainer Script 없음 |
| SHA256 및 패키지 md5sums 재검증, git diff --check | PASS |

산출물: `kcf-tools_0.15.0-1+ubuntu24.04_amd64.deb`, **784,686 bytes**.
SHA256: `1b76bd756972996b3f8901349c883c70fb399f88c584697390a1879311e9bc7b`.
실제 Depends는 `qt6-qpa-plugins`, `libc6 (>= 2.38)`, `libgcc-s1 (>= 3.0)`,
`libqt6core6t64 (>= 6.4.0)`, `libqt6gui6t64 (>= 6.4.0)`,
`libqt6network6t64 (>= 6.1.2)`, `libqt6widgets6t64 (>= 6.3.0)`, `libstdc++6 (>= 12)`다.

산출물·로그는 `/tmp/kcf-tools-015-release`의 `packages/`, `verification.json`,
`configure.log`, `build.log`, `build-first-failed.log`, `cpack.log`, `gui-smoke.log`에 있다.
이 경로는 당시 기록이며 영구 배포 저장소나 다른 환경의 필수 경로가 아니다.
실제 설치·업그레이드, 이번 Release의 데스크톱 GUI·물리 LAN·ARM64 및 Release 전체 CTest는 **미검증**이다.
과거 Local GUI 사용자 확인 PASS 및 v0.14 이력은 별도이며 이 패키지 검증으로 소급하지 않는다.
Tag/Release는 생성하지 않았다. 이번 문서 작업에서는 빌드·회귀·패키징을 재실행하지 않았다.

Historical: 앞선 Core 5.6.1 Debug 부분 회귀 7/7, Mock/stub 6/6과 Core 5.6.0 Configure 거부 PASS는
당시 결과다. 일반 실제 Backend는 Core >=5.6.1 및 필수 API, 패키징은 정확히 5.6.1을 요구한다.

## 빌드

배포 대상 Debian/Ubuntu의 Qt6 Widgets/Network 개발 패키지, QPA plugins,
nlohmann-json3-dev, C++17 compiler, CMake, dpkg-dev가 필요하다.
Qt 온라인 설치 프로그램의 `/opt/Qt` SDK를 배포판 Qt 패키지로 간주하지 않는다.
해당 SDK로 링크한 바이너리에 배포판 Qt 의존성을 임의로 붙이거나 시스템 라이브러리를 복사하지 않는다.

### 추출 SDK와 배포판 Qt staging

현재 Tools CMake는 `KCF_SOURCE_DIR/kcf`를 add_subdirectory로 읽는다. 추출 SDK에는 소스 트리가
없으므로 `/tmp`의 어댑터에서 패키지의 `KCF::core`를 기존 `kcf_core` 이름으로 연결한다.
버전·API 검사는 그대로 통과해야 하며 시스템 SDK fallback을 사용하지 않는다.

최초 어댑터는 imported target이 하위 디렉터리에만 보이는 문제로 SDK include가 전달되지 않아
시스템 헤더의 Snapshot API 누락으로 빌드가 실패했다. `IMPORTED_GLOBAL TRUE`를 설정한 뒤
추출 SDK include/정적 archive의 절대 경로로 컴파일·링크되어 PASS했다. 최종 dependency 파일에서도
시스템 `/usr/include/kcf` 미사용을 확인했다. 이 수정은 `/tmp` 어댑터에만 적용했다.

다음은 저장소 루트에서 실행하는 재현 절차다. `CORE_DEB`는 실제 입력 파일로 바꾸고
배포판·아키텍처·패키지 버전을 먼저 확인한다. 새 작업 경로를 사용하며 기존 Debug 빌드를 지우지 않는다.
아래 명령은 문서 예시로, 이번 문서 작업에서 실행하지 않았다.

```sh
CORE_DEB=/path/to/kcf-core_5.6.1-1+ubuntu24.04_amd64.deb
TOOLS_STAGE=$(mktemp -d /tmp/kcf-tools-015-release.XXXXXXXX)
export TOOLS_STAGE
printf '%s  %s\n' b057ef8c02e600aa563b69f7f1a5c5dc7c9168c89b6910b35ccbeef22b710fa5 "$CORE_DEB" | sha256sum -c -
dpkg-deb -x "$CORE_DEB" "$TOOLS_STAGE/core-sdk"
mkdir -p "$TOOLS_STAGE/staging/kcf" "$TOOLS_STAGE/qt-debs" "$TOOLS_STAGE/bin"
ln -s "$TOOLS_STAGE/core-sdk/usr/include" "$TOOLS_STAGE/staging/kcf/include"
cat > "$TOOLS_STAGE/staging/kcf/CMakeLists.txt" <<'CMAKE'
find_package(KCF 5.6.1 EXACT CONFIG REQUIRED
  PATHS "${CMAKE_CURRENT_LIST_DIR}/../../core-sdk/usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}/cmake/KCF"
  NO_DEFAULT_PATH)
set_property(TARGET KCF::core PROPERTY IMPORTED_GLOBAL TRUE)
add_library(kcf_core ALIAS KCF::core)
CMAKE
```

검증 환경은 배포판 Qt 런타임이 이미 설치되어 있었고 개발 패키지만 없었다.
`apt-get download`와 `dpkg-deb -x`로 개발 파일을 staging했으며 설치·업그레이드하지 않았다.
다운로드 후보가 **6.4.2+dfsg-21.1build5**이고 설치 런타임과 일치하는지 확인해야 한다.
다른 버전이라면 임의로 혼용하지 않는다. `/opt/Qt/6.9.3`은 Debug 개발용이며 아래 Release에는 사용하지 않는다.

```sh
(cd "$TOOLS_STAGE/qt-debs" && apt-get download qt6-base-dev qt6-base-dev-tools qmake6 qmake6-bin)
for deb in "$TOOLS_STAGE"/qt-debs/*.deb; do
  dpkg-deb -x "$deb" "$TOOLS_STAGE/qt-root"
done
python3 - <<'PYQT'
import os, subprocess
from pathlib import Path
root = Path(os.environ['TOOLS_STAGE'])
arch = subprocess.check_output(['dpkg-architecture', '-qDEB_HOST_MULTIARCH'], text=True).strip()
for library in (Path('/usr/lib') / arch).glob('libQt6*'):
    dest = root / 'qt-root/usr/lib' / arch / library.name
    if not dest.exists() and not dest.is_symlink():
        dest.symlink_to(library)
PYQT
cat > "$TOOLS_STAGE/bin/dpkg-query" <<'SH'
#!/bin/sh
if [ "$1" = "--search" ] && [ "$#" = 2 ]; then
  resolved=$(readlink -f -- "$2") || exit 1
  exec /usr/bin/dpkg-query --search "$resolved"
fi
exec /usr/bin/dpkg-query "$@"
SH
chmod 755 "$TOOLS_STAGE/bin/dpkg-query"
```

이 wrapper는 Qt staging symlink를 실제 설치 라이브러리로 해석하여 시스템 dpkg의 소유 패키지
검사를 수행한다. 패키지 소유 검사를 생략하거나 성공을 조작하지 않는다. Qt 런타임이 없거나
소유 패키지를 확인할 수 없으면 중단해야 한다. 재현 환경에 따라 추가 개발 의존성이 필요할 수 있다.

```sh
cmake -S . -B "$TOOLS_STAGE/build" \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr \
  -DBUILD_SHARED_LIBS=OFF -DKCF_SOURCE_DIR="$TOOLS_STAGE/staging" \
  -DKCF_TOOL_BUILD_GUI=ON -DKCF_TOOL_BUILD_TESTS=OFF \
  -DKCF_TOOL_ENABLE_PACKAGING=ON \
  -DCMAKE_PREFIX_PATH="$TOOLS_STAGE/qt-root/usr" \
  -DKCF_DPKG_QUERY="$TOOLS_STAGE/bin/dpkg-query"
cmake --build "$TOOLS_STAGE/build" -j2
cpack --config "$TOOLS_STAGE/build/CPackConfig.cmake" -B "$TOOLS_STAGE/packages"
TOOLS_DEB="$TOOLS_STAGE/packages/kcf-tools_0.15.0-1+ubuntu24.04_amd64.deb"
dpkg-deb --info "$TOOLS_DEB"
dpkg-deb --contents "$TOOLS_DEB"
dpkg-deb -x "$TOOLS_DEB" "$TOOLS_STAGE/extracted"
dpkg-deb -e "$TOOLS_DEB" "$TOOLS_STAGE/control"
QT_QPA_PLATFORM=offscreen "$TOOLS_STAGE/extracted/usr/bin/kcf_tool" --help
readelf -d "$TOOLS_STAGE/extracted/usr/bin/kcf_tool"
ldd "$TOOLS_STAGE/extracted/usr/bin/kcf_tool"
sha256sum "$TOOLS_DEB" > "$TOOLS_STAGE/packages/SHA256SUMS"
sha256sum -c "$TOOLS_STAGE/packages/SHA256SUMS"
```

빌드 시각 등으로 재생성 DEB의 SHA256은 달라질 수 있다. 위 기록은 실제 생성한 산출물의 해시다.
`amd64`는 빌드 환경의 예시이며 ARM64 빌드 성공을 의미하지 않는다.
패키징을 켜면 실제 Backend, GUI, 정적 Core 및 FrameworkVersion 5.6.1을 검사한다.
실제 Backend 전체 컴파일과 회귀 테스트로 API 연동을 추가 확인해야 한다.
패키징 OFF(기본값)에서는 기존 Mock/stub, GUI OFF 및 직접 소스 링크 빌드를 유지한다.

생성 후 별도 검증 장비에서 설치·패키지 상태·실제 Backend를 확인합니다(이번에는 미실행).

```sh
sudo apt install "$TOOLS_DEB"
dpkg-query -W -f='${Package} ${Version} ${Architecture}\n' kcf-tools
/usr/bin/kcf_tool --help
QT_QPA_PLATFORM=offscreen /usr/bin/kcf_tool --backend kcf --no-network
```

GUI는 종료할 때 Ctrl+C를 사용합니다. offscreen startup은 물리 LAN/데스크톱 검증이 아닙니다.

## 의존성과 제외 항목

소스 연결 빌드에서는 `KCF_SOURCE_DIR/kcf`를 빌드하고, 이번 SDK staging은 추출한 정적 Core를 링크한다.
두 방식 모두 GUI에 Core를 정적으로 포함하므로 `kcf-core` DEB는
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

## Historical — 2026-11-11 개발 VM 검증

다음은 Tools v0.14 / Core 5.6.0 당시 기록이며 v0.15 패키징 PASS가 아닙니다.


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
