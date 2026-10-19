include(cmake/feature-sparkle.cmake)

find_package(nlohmann_json 3.11 REQUIRED)

target_sources(
  obs-studio
  PRIVATE
    dialogs/OBSPermissions.cpp
    dialogs/OBSPermissions.hpp
    forms/OBSPermissions.ui
    utility/CrashHandler_MacOS.mm
    utility/NativeEventFilter.cpp
    utility/platform-osx.mm
    utility/system-info-macos.mm
    utility/AutoUpdateThread.cpp
    utility/AutoUpdateThread.hpp
)
target_compile_options(obs-studio PRIVATE -Wno-quoted-include-in-framework-header -Wno-comma)

list(APPEND _frontend_objcxx_compile_options -fobjc-arc -fmodules -fcxx-modules)

set_property(
  SOURCE utility/platform-osx.mm utility/CrashHandler_MacOS.mm
  APPEND
  PROPERTY COMPILE_OPTIONS ${_frontend_objcxx_compile_options}
)

if(CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL 14.0.3)
  target_compile_options(obs-studio PRIVATE -Wno-error=unqualified-std-cast-call)
endif()

target_link_libraries(
  obs-studio
  PRIVATE
    "$<LINK_LIBRARY:FRAMEWORK,AppKit.framework>"
    "$<LINK_LIBRARY:FRAMEWORK,ApplicationServices.framework>"
    "$<LINK_LIBRARY:FRAMEWORK,AVFoundation.framework>"
    "$<LINK_LIBRARY:FRAMEWORK,Security.framework>"
    nlohmann_json::nlohmann_json
)
