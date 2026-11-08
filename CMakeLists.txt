# CheatChord desktop: VST3 (Windows, macOS, Linux), AU (macOS) and a standalone app, from the MPC build's engine.
cmake_minimum_required (VERSION 3.22)

set (CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Oldest macOS")
project (CheatChord VERSION 4.1.0 LANGUAGES C CXX)

set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)
set (CMAKE_C_STANDARD 11)
set (CMAKE_C_STANDARD_REQUIRED ON)
set (CMAKE_POSITION_INDEPENDENT_CODE ON)
if (MSVC)
    # one runtime for engine and plugin: static, so the DLL needs no Visual C++ redistributable
    set (CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif ()

set (CC_JUCE_DIR "" CACHE PATH "A JUCE checkout (else JUCE 8.0.10 is downloaded)")
set (CC_FORMATS "" CACHE STRING "Plugin formats (default: VST3 Standalone, + AU on macOS)")
option (CC_BUILD_HOST_TEST "Build the test host that loads the VST3" OFF)
# VST2 (for MPC 2 desktop, which loads only VST2 on Windows): built on vst2abi/, a clean-room description of the ABI
option (CC_VST2 "Also build a VST2 plugin (MPC 2 desktop)" OFF)

if (CC_JUCE_DIR)
    add_subdirectory ("${CC_JUCE_DIR}" JUCE EXCLUDE_FROM_ALL)
else ()
    include (FetchContent)
    FetchContent_Declare (JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.10 GIT_SHALLOW ON)
    FetchContent_MakeAvailable (JUCE)
endif ()

if (NOT CC_FORMATS)
    set (CC_FORMATS VST3 Standalone)
    if (APPLE)
        list (APPEND CC_FORMATS AU)
    endif ()
endif ()
if (CC_VST2)
    list (APPEND CC_FORMATS VST)
    juce_set_vst2_sdk_path ("${CMAKE_CURRENT_SOURCE_DIR}/vst2abi")
endif ()

# ---------------------------------------------------------------- the engine (C11, the same code as the MPC build)
add_library (cc_engine STATIC
    engine/chordpilot.c engine/harmony.c engine/theory.c engine/voicing.c engine/synth.c
    engine/seq_out.c engine/library.c engine/artists.c engine/refs.c engine/compat.c)
target_include_directories (cc_engine PUBLIC engine)
if (MSVC)
    target_compile_options (cc_engine PRIVATE /W3 /wd4244 /wd4267 /wd4996 /wd4305 /utf-8)
    target_compile_definitions (cc_engine PRIVATE _CRT_SECURE_NO_WARNINGS)
else ()
    target_compile_options (cc_engine PRIVATE -O2 -Wall -Wno-unused-parameter -Wno-unused-function)
endif ()
if (UNIX AND NOT APPLE)
    find_package (Threads REQUIRED)
    target_link_libraries (cc_engine PUBLIC Threads::Threads m)
    target_compile_definitions (cc_engine PRIVATE _GNU_SOURCE)
endif ()

# ---------------------------------------------------------------- the plugin
juce_add_plugin (CheatChord
    COMPANY_NAME "JJ DA PLUG"
    COMPANY_WEBSITE ""
    BUNDLE_ID com.jjdaplug.cheatchord
    PLUGIN_MANUFACTURER_CODE JjDp
    PLUGIN_CODE ChtC
    PRODUCT_NAME "CheatChord"
    DESCRIPTION "Chord progression co-pilot"
    FORMATS ${CC_FORMATS}
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Generator
    VST2_CATEGORY kPlugCategSynth
    VST_NUM_MIDI_INS 16
    VST_NUM_MIDI_OUTS 16
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    MICROPHONE_PERMISSION_ENABLED FALSE)

juce_add_binary_data (CheatChordData SOURCES
    assets/fonts/TitilliumWeb-Regular.ttf
    assets/fonts/TitilliumWeb-SemiBold.ttf
    assets/fonts/TitilliumWeb-Bold.ttf
    assets/logo_big.png
    assets/src_medallion.png
    assets/jf_velvet.png)

target_sources (CheatChord PRIVATE
    plugin/CCEngine.cpp
    plugin/MidiPort.cpp
    plugin/PluginProcessor.cpp
    plugin/PluginEditor.cpp
    plugin/ui/Theme.cpp
    plugin/ui/Ui.cpp
    plugin/ui/Panels.cpp
    plugin/ui/Layouts.cpp)

target_compile_definitions (CheatChord PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_STANDALONE_FILTER_WINDOW_USE_KIOSK_MODE=0)

if (UNIX AND NOT APPLE AND CC_LINUX_NO_XEXT)   # a build machine without ALSA headers (the CI machines have them)
    target_compile_definitions (CheatChord PUBLIC JUCE_ALSA=0 JUCE_JACK=0)
endif ()

target_link_libraries (CheatChord
    PRIVATE
        CheatChordData
        cc_engine
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------- the test host (loads the built VST3: audio, MIDI, state, screenshots)
if (CC_BUILD_HOST_TEST)
    juce_add_console_app (cc_host_test PRODUCT_NAME "cc_host_test")
    target_sources (cc_host_test PRIVATE tests/host_test.cpp)
    target_compile_definitions (cc_host_test PRIVATE JUCE_PLUGINHOST_VST3=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    if (UNIX AND NOT APPLE AND CC_LINUX_NO_XEXT)
        target_compile_definitions (cc_host_test PRIVATE JUCE_ALSA=0 JUCE_JACK=0)
    endif ()
    target_link_libraries (cc_host_test PRIVATE juce::juce_audio_processors juce::juce_gui_extra juce::juce_audio_formats
                           PUBLIC juce::juce_recommended_config_flags)
endif ()

# ---------------------------------------------------------------- screenshots of every layout (links the plugin's code directly)
if (CC_BUILD_HOST_TEST)
    juce_add_console_app (cc_ui_shots PRODUCT_NAME "cc_ui_shots")
    target_sources (cc_ui_shots PRIVATE tests/ui_shots.cpp)
    target_include_directories (cc_ui_shots PRIVATE $<TARGET_PROPERTY:CheatChord,INCLUDE_DIRECTORIES>)
    target_compile_definitions (cc_ui_shots PRIVATE $<TARGET_PROPERTY:CheatChord,COMPILE_DEFINITIONS>)
    target_link_libraries (cc_ui_shots PRIVATE CheatChord)

    juce_add_console_app (cc_port_test PRODUCT_NAME "cc_port_test")
    target_sources (cc_port_test PRIVATE tests/port_test.cpp)
    target_include_directories (cc_port_test PRIVATE $<TARGET_PROPERTY:CheatChord,INCLUDE_DIRECTORIES>)
    target_compile_definitions (cc_port_test PRIVATE $<TARGET_PROPERTY:CheatChord,COMPILE_DEFINITIONS>)
    target_link_libraries (cc_port_test PRIVATE CheatChord)
endif ()
