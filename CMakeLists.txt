cmake_minimum_required(VERSION 4.2)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

set(TCA_TWITCH_CLIENT_ID "" CACHE STRING "Publisher's public Twitch application Client ID")
if(NOT TCA_TWITCH_CLIENT_ID MATCHES "^[a-zA-Z0-9]*$")
  message(FATAL_ERROR "TCA_TWITCH_CLIENT_ID must contain only letters and digits (never a secret)")
endif()
configure_file(src/release-config.hpp.in generated/release-config.hpp @ONLY)
include_directories("${CMAKE_CURRENT_BINARY_DIR}/generated")

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" ON)
option(ENABLE_QT "Use Qt functionality" ON)
if(NOT WIN32)
  message(FATAL_ERROR "Twitch Category Assistant currently supports Windows x64 only")
endif()

include(compilerconfig)
include(defaults)
include(helpers)

add_library(${CMAKE_PROJECT_NAME} MODULE)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
endif()

if(ENABLE_QT)
  find_package(Qt6 6.11 COMPONENTS Widgets Core Network Concurrent REQUIRED)
  if(NOT TARGET Qt6::QSchannelBackendPlugin)
    message(FATAL_ERROR "The matching Qt Schannel TLS plugin is required for HTTPS")
  endif()
  install(FILES "$<TARGET_FILE:Qt6::QSchannelBackendPlugin>"
    DESTINATION "${CMAKE_PROJECT_NAME}/data/qt-plugins/tls")
  install(FILES "${Qt6_DIR}/../../../licenses/qt6/LICENSE.GPL2"
    DESTINATION "${CMAKE_PROJECT_NAME}/data/qt-plugins" RENAME Qt-GPL-2.0.txt)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets Qt6::Network Qt6::Concurrent Advapi32)
  target_compile_options(
    ${CMAKE_PROJECT_NAME}
    PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header -Wno-comma>
  )
  set_target_properties(
    ${CMAKE_PROJECT_NAME}
    PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON
  )
endif()

target_sources(${CMAKE_PROJECT_NAME} PRIVATE
  src/plugin/plugin.cpp src/ui/dock.cpp src/ui/dock.hpp
  src/twitch/client.cpp src/twitch/client.hpp
  src/settings/settings.cpp src/settings/settings.hpp
  src/security/credentials.cpp src/security/credentials.hpp
  src/detection/detector.cpp src/detection/detector.hpp)
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE src)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)

include(CTest)
if(BUILD_TESTING)
  add_executable(tca-tests tests/tests.cpp src/settings/settings.cpp src/detection/detector.cpp
    src/twitch/client.cpp src/security/credentials.cpp)
  set_target_properties(tca-tests PROPERTIES AUTOMOC ON)
  target_include_directories(tca-tests PRIVATE src)
  target_compile_definitions(tca-tests PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
  target_link_libraries(tca-tests PRIVATE Qt6::Core Qt6::Network Qt6::Concurrent Advapi32)
  add_test(NAME tca-tests COMMAND tca-tests)
  add_executable(tca-ui-check tests/ui-check.cpp src/ui/dock.cpp src/settings/settings.cpp src/detection/detector.cpp
    src/twitch/client.cpp src/security/credentials.cpp)
  set_target_properties(tca-ui-check PROPERTIES AUTOMOC ON)
  target_include_directories(tca-ui-check PRIVATE src)
  target_compile_definitions(tca-ui-check PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
  target_link_libraries(tca-ui-check PRIVATE Qt6::Widgets Qt6::Network Qt6::Concurrent Advapi32)
  add_test(NAME tca-ui-check COMMAND tca-ui-check -platform minimal)
  add_executable(tca-tls-check tests/tls-check.cpp src/twitch/client.cpp src/settings/settings.cpp src/security/credentials.cpp)
  set_target_properties(tca-tls-check PROPERTIES AUTOMOC ON)
  target_include_directories(tca-tls-check PRIVATE src)
  target_link_libraries(tca-tls-check PRIVATE Qt6::Core Qt6::Network Advapi32)
  add_custom_command(TARGET tca-tls-check POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/tls-runtime/tls"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:Qt6::QSchannelBackendPlugin>" "${CMAKE_CURRENT_BINARY_DIR}/tls-runtime/tls")
  add_test(NAME tca-tls-missing COMMAND tca-tls-check --expect-unavailable)
  add_test(NAME tca-tls-packaged COMMAND tca-tls-check "${CMAKE_CURRENT_BINARY_DIR}/tls-runtime")
endif()

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})
