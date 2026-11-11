include(${CMAKE_CURRENT_LIST_DIR}/PackagingEnvironment.cmake)
kcf_detect_package_environment(Tools)

if(NOT KCF_TOOL_BUILD_GUI OR NOT KCF_SOURCE_DIR)
    message(FATAL_ERROR "DEB packaging requires GUI=ON and KCF_SOURCE_DIR (real Backend)")
endif()
if(BUILD_SHARED_LIBS)
    message(FATAL_ERROR "Phase-1 DEB packaging requires BUILD_SHARED_LIBS=OFF (static KCF Core)")
endif()
if(NOT CMAKE_INSTALL_PREFIX STREQUAL "/usr")
    message(FATAL_ERROR "DEB packaging requires CMAKE_INSTALL_PREFIX=/usr")
endif()
find_program(KCF_DPKG_QUERY dpkg-query)
find_program(KCF_DPKG_SHLIBDEPS dpkg-shlibdeps)
if(NOT KCF_DPKG_QUERY OR NOT KCF_DPKG_SHLIBDEPS)
    message(FATAL_ERROR "DEB packaging requires dpkg-query and dpkg-shlibdeps (dpkg-dev)")
endif()
# CPack may ignore libraries without package metadata. Reject an online Qt SDK
# here instead of silently producing a DEB with missing Qt dependencies.
foreach(module Core Gui Widgets Network)
    get_target_property(qt_library Qt6::${module} LOCATION)
    execute_process(COMMAND "${KCF_DPKG_QUERY}" --search "${qt_library}"
        RESULT_VARIABLE qt_package_result OUTPUT_QUIET ERROR_QUIET)
    if(NOT qt_package_result EQUAL 0)
        message(FATAL_ERROR "Qt6::${module} must come from a Debian/Ubuntu package: ${qt_library}")
    endif()
endforeach()
include(CheckCXXSourceCompiles)
include(CMakePushCheckState)
cmake_push_check_state(RESET)
set(CMAKE_REQUIRED_INCLUDES "${KCF_SOURCE_DIR}/kcf/include")
# Do not reuse the result after the selected Core source has changed.
unset(KCF_TOOL_PACKAGE_CORE_560 CACHE)
check_cxx_source_compiles("#include <kcf/network/compatibility.hpp>
constexpr kcf::network::FrameworkVersion version{};
static_assert(version.major == 5 && version.minor == 6 && version.patch == 0,
              \"Phase-1 packaging requires KCF 5.6.0\");
int main() { return 0; }" KCF_TOOL_PACKAGE_CORE_560)
cmake_pop_check_state()
if(NOT KCF_TOOL_PACKAGE_CORE_560 OR NOT KCF_TOOL_REMOTE_CONTROL OR NOT KCF_TOOL_NETWORK_DIAGNOSTICS)
    message(FATAL_ERROR "Package requires KCF 5.6.0 with Remote Control/Topic and Diagnostics APIs")
endif()
set(CPACK_GENERATOR DEB)
set(CMAKE_INSTALL_DEFAULT_DIRECTORY_PERMISSIONS
    OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
set(CPACK_PACKAGE_NAME kcf-tools)
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_VENDOR Nyamkani)
set(CPACK_PACKAGE_CONTACT "Nyamkani <Nyamkani@users.noreply.github.com>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "KCF Tools Qt6 GUI with the KCF 5.6.0 real Backend")
set(CPACK_DEBIAN_PACKAGE_SECTION devel)
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/Nyamkani/kcf_tools")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
# Qt's platform plugins are loaded at runtime and are not found by shlibdeps.
set(CPACK_DEBIAN_PACKAGE_DEPENDS "qt6-qpa-plugins")
set(CPACK_PACKAGING_INSTALL_PREFIX /usr)
# Core is compiled into kcf_tool; no runtime dependency on the kcf-core DEB.
# Qt/system shared objects must come from distribution packages, never bundled.
include(CPack)
