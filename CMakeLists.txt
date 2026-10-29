cmake_minimum_required(VERSION 3.16)
project(RasterDamage VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Where the Steinberg VST3 SDK lives (MIT licensed, https://github.com/steinbergmedia/vst3sdk)
set(VST3_SDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/vst3sdk" CACHE PATH "VST3 SDK root")
if(NOT EXISTS "${VST3_SDK_ROOT}/pluginterfaces/base/funknown.h")
  message(FATAL_ERROR "VST3 SDK not found at ${VST3_SDK_ROOT}. Run scripts/get_sdk.sh (or .bat) first.")
endif()

option(RD_BUILD_TOOLS "Build the offline renderer and the DSP test program" ON)
option(RD_WITH_GUI "Build the custom VSTGUI editor (otherwise the host shows its generic UI)" ON)

# ---------------------------------------------------------------------------
# DSP-only tools (no SDK needed). These let you audition presets without a DAW
# and prove the engine behaves.
# ---------------------------------------------------------------------------
if(RD_BUILD_TOOLS)
  add_executable(rd_test tests/test_engine.cpp)
  target_include_directories(rd_test PRIVATE src)
  add_executable(rd_render tools/rd_render.cpp)
  target_include_directories(rd_render PRIVATE src)
  if(MINGW)
    target_link_options(rd_test PRIVATE -static)
    target_link_options(rd_render PRIVATE -static)
  endif()
endif()

# ---------------------------------------------------------------------------
# The plugin itself. Compiles the needed SDK sources straight into the module.
# ---------------------------------------------------------------------------
set(SDK ${VST3_SDK_ROOT})
set(SDK_SOURCES
  ${SDK}/base/source/baseiids.cpp
  ${SDK}/base/source/fbuffer.cpp
  ${SDK}/base/source/fdebug.cpp
  ${SDK}/base/source/fdynlib.cpp
  ${SDK}/base/source/fobject.cpp
  ${SDK}/base/source/fstreamer.cpp
  ${SDK}/base/source/fstring.cpp
  ${SDK}/base/source/timer.cpp
  ${SDK}/base/source/updatehandler.cpp
  ${SDK}/base/thread/source/fcondition.cpp
  ${SDK}/base/thread/source/flock.cpp
  ${SDK}/pluginterfaces/base/conststringtable.cpp
  ${SDK}/pluginterfaces/base/coreiids.cpp
  ${SDK}/pluginterfaces/base/funknown.cpp
  ${SDK}/pluginterfaces/base/ustring.cpp
  ${SDK}/public.sdk/source/common/commoniids.cpp
  ${SDK}/public.sdk/source/common/pluginview.cpp
  ${SDK}/public.sdk/source/main/pluginfactory.cpp
  ${SDK}/public.sdk/source/main/moduleinit.cpp
  ${SDK}/public.sdk/source/vst/vstaudioeffect.cpp
  ${SDK}/public.sdk/source/vst/vstbus.cpp
  ${SDK}/public.sdk/source/vst/vstcomponent.cpp
  ${SDK}/public.sdk/source/vst/vstcomponentbase.cpp
  ${SDK}/public.sdk/source/vst/vsteditcontroller.cpp
  ${SDK}/public.sdk/source/vst/vstinitiids.cpp
  ${SDK}/public.sdk/source/vst/vstparameters.cpp
)

if(WIN32)
  list(APPEND SDK_SOURCES ${SDK}/public.sdk/source/main/dllmain.cpp)
elseif(APPLE)
  list(APPEND SDK_SOURCES ${SDK}/public.sdk/source/main/macmain.cpp)
else()
  list(APPEND SDK_SOURCES ${SDK}/public.sdk/source/main/linuxmain.cpp)
endif()


# ---------------------------------------------------------------------------
# Custom editor (VSTGUI, from the SDK download). Windows: Direct2D, Linux: cairo/X11.
# ---------------------------------------------------------------------------
if(RD_WITH_GUI)
  set(VSTGUI_ROOT ${SDK}/vstgui4)
  set(GUI_SOURCES
    src/gui/RasterUI.cpp
    src/gui/RasterEditor.cpp
    ${SDK}/public.sdk/source/vst/vstguieditor.cpp
  )
  if(WIN32)
    if(MINGW)
      # MinGW lacks Microsoft's PPL and the newest DirectComposition/DirectWrite headers;
      # this unity file swaps in portable replacements (see src/gui/).
      list(APPEND GUI_SOURCES src/gui/vstgui_win32_mingw.cpp)
    else()
      list(APPEND GUI_SOURCES ${VSTGUI_ROOT}/vstgui/vstgui_win32.cpp)
    endif()
    if(EXISTS ${SDK}/public.sdk/source/vst/vstgui_win32_bundle_support.cpp)
      list(APPEND GUI_SOURCES ${SDK}/public.sdk/source/vst/vstgui_win32_bundle_support.cpp)
    endif()
  elseif(APPLE)
    message(WARNING "macOS GUI: VSTGUI needs its Objective-C++ files; building without the custom editor.")
    set(GUI_SOURCES "")
  else()
    list(APPEND GUI_SOURCES ${VSTGUI_ROOT}/vstgui/vstgui_linux.cpp
                            ${SDK}/public.sdk/source/vst/vstgui_linux_runloop_support.cpp)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(RD_LINUX_GUI REQUIRED IMPORTED_TARGET cairo pangocairo pangoft2 freetype2 fontconfig
                      xcb xcb-util xcb-cursor xcb-keysyms xcb-xkb xkbcommon xkbcommon-x11 glib-2.0)
  endif()
endif()

add_library(RasterDamage MODULE
  src/RasterEntry.cpp
  src/RasterProcessor.cpp
  src/RasterController.cpp
  ${SDK_SOURCES}
  ${GUI_SOURCES}
)
target_include_directories(RasterDamage PRIVATE src src/gui ${SDK})
if(RD_WITH_GUI AND GUI_SOURCES)
  target_include_directories(RasterDamage PRIVATE ${SDK}/vstgui4)
  target_compile_definitions(RasterDamage PRIVATE RD_WITH_GUI=1)
  if(WIN32)
    target_compile_definitions(RasterDamage PRIVATE UNICODE _UNICODE _WIN32_WINNT=0x0A00)
  endif()
endif()
target_compile_definitions(RasterDamage PRIVATE
  $<$<CONFIG:Debug>:DEVELOPMENT=1>
  $<$<NOT:$<CONFIG:Debug>>:RELEASE=1>)

if(MSVC)
  target_compile_options(RasterDamage PRIVATE /O2 /fp:precise /EHsc /wd4244 /wd4267 /wd4996)
  target_compile_definitions(RasterDamage PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
else()
  target_compile_options(RasterDamage PRIVATE -O2 -fno-strict-aliasing)
  if(WIN32)
    # MinGW: ship one self-contained DLL with no libgcc / libstdc++ / winpthread DLLs
    target_link_options(RasterDamage PRIVATE -static -static-libgcc -static-libstdc++)
    target_compile_definitions(RasterDamage PRIVATE NOMINMAX)
  endif()
endif()

set_target_properties(RasterDamage PROPERTIES PREFIX "" OUTPUT_NAME "RasterDamage")

if(WIN32)
  set_target_properties(RasterDamage PROPERTIES SUFFIX ".vst3")
  if(RD_WITH_GUI AND GUI_SOURCES)
    target_link_libraries(RasterDamage PRIVATE d2d1 dwrite windowscodecs gdi32 comdlg32 ole32 oleaut32 uuid shlwapi user32 shell32 dwmapi imm32 winmm opengl32)
  endif()
elseif(APPLE)
  set_target_properties(RasterDamage PROPERTIES
    BUNDLE TRUE BUNDLE_EXTENSION "vst3"
    MACOSX_BUNDLE_BUNDLE_NAME "RasterDamage")
  target_link_libraries(RasterDamage PRIVATE "-framework CoreFoundation")
else()
  set_target_properties(RasterDamage PROPERTIES SUFFIX ".so")
  target_link_libraries(RasterDamage PRIVATE dl pthread)
  if(RD_WITH_GUI AND GUI_SOURCES)
    target_link_libraries(RasterDamage PRIVATE PkgConfig::RD_LINUX_GUI)
  endif()
endif()

# Controller behaviour test (fake host; links the SDK core but not the plugin entry or GUI).
set(CORE_SOURCES ${SDK_SOURCES})
list(FILTER CORE_SOURCES EXCLUDE REGEX "dllmain|linuxmain|macmain|pluginfactory|moduleinit")
if(RD_BUILD_TOOLS AND NOT CMAKE_CROSSCOMPILING)
  add_executable(rd_test_controller tests/test_controller.cpp src/RasterController.cpp
                 ${CORE_SOURCES} ${SDK}/public.sdk/source/common/memorystream.cpp)
  target_include_directories(rd_test_controller PRIVATE src ${SDK})
  target_link_libraries(rd_test_controller PRIVATE dl pthread)
endif()

# Handy copy of the built plugin into a clean "dist" folder
add_custom_command(TARGET RasterDamage POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/dist"
  COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:RasterDamage>" "${CMAKE_BINARY_DIR}/dist/"
  VERBATIM)
