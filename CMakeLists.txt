cmake_minimum_required(VERSION 3.22)
project(TRACKSPACE VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
option(TRACKSPACE_BUILD_PLUGIN "Build the VST3 plugin" ON)
include(CTest)
add_library(trackspace_dsp STATIC Source/SpectralEngine.cpp)
target_include_directories(trackspace_dsp PUBLIC Source)
set_target_properties(trackspace_dsp PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(BUILD_TESTING)
    add_executable(dsp_tests Tests/DspTests.cpp)
    target_link_libraries(dsp_tests PRIVATE trackspace_dsp)
    add_test(NAME spectral_audio_tests COMMAND dsp_tests)
endif()
if(TRACKSPACE_BUILD_PLUGIN)
    if(TRACKSPACE_HOST_JUCEAIDE AND JUCE_SOURCE_DIR)
        # Cross builds need a runnable host tool, not a Windows executable.
        set(JUCE_MODULES_ONLY ON)
        add_subdirectory("${JUCE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/JUCE")
        include("${JUCE_SOURCE_DIR}/extras/Build/CMake/JUCEUtils.cmake")
        add_executable(juce::juceaide IMPORTED GLOBAL)
        set_target_properties(juce::juceaide PROPERTIES IMPORTED_LOCATION "${TRACKSPACE_HOST_JUCEAIDE}")
    elseif(TRACKSPACE_JUCE_PACKAGE)
        find_package(JUCE 7.0.12 CONFIG REQUIRED)
    elseif(JUCE_SOURCE_DIR)
        add_subdirectory("${JUCE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/JUCE")
    else()
        include(FetchContent)
        FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
            GIT_TAG 4f43011b96eb0636104cb3e433894cda98243626)
        FetchContent_MakeAvailable(JUCE)
    endif()
    juce_add_binary_data(TrackspaceAssets NAMESPACE Assets SOURCES Assets/normal.png Assets/advanced.png)
    juce_add_plugin(Trackspace
        COMPANY_NAME "TRACKSPACE" BUNDLE_ID "audio.trackspace.spectral"
        PLUGIN_MANUFACTURER_CODE Trks PLUGIN_CODE Ts01
        PRODUCT_NAME "TRACKSPACE" FORMATS VST3
        IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE VST3_AUTO_MANIFEST FALSE
        VST3_CATEGORIES Fx Dynamics EQ)
    target_sources(Trackspace PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_compile_definitions(Trackspace PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
    target_link_libraries(Trackspace PRIVATE trackspace_dsp TrackspaceAssets
        juce::juce_audio_processors juce::juce_gui_basics
        PUBLIC juce::juce_recommended_config_flags)
    if(MINGW)
        target_link_options(Trackspace_VST3 PRIVATE -static-libgcc -static-libstdc++ -static)
    endif()
    if(BUILD_TESTING)
        juce_add_console_app(processor_tests PRODUCT_NAME "TRACKSPACE Processor Tests")
        target_sources(processor_tests PRIVATE Tests/ProcessorTests.cpp)
        target_include_directories(processor_tests PRIVATE Source "${JUCE_MODULES_DIR}")
        target_link_libraries(processor_tests PRIVATE Trackspace)
        add_test(NAME host_processor_tests COMMAND processor_tests)
        if(WIN32)
            get_target_property(_modulePath juce::juce_audio_processors INTERFACE_JUCE_MODULE_PATH)
            set(_sdk "${_modulePath}/juce_audio_processors/format_types/VST3_SDK")
            add_executable(vst3_smoke Tests/Vst3Smoke.cpp
                "${_sdk}/pluginterfaces/base/funknown.cpp"
                "${_sdk}/pluginterfaces/base/coreiids.cpp"
                "${_sdk}/public.sdk/source/vst/vstinitiids.cpp")
            target_include_directories(vst3_smoke PRIVATE "${_sdk}")
            if(MINGW)
                target_link_options(vst3_smoke PRIVATE -static-libgcc -static-libstdc++ -static)
                target_link_options(dsp_tests PRIVATE -static-libgcc -static-libstdc++ -static)
                target_link_options(processor_tests PRIVATE -static-libgcc -static-libstdc++ -static)
            endif()
            add_test(NAME vst3_binary_load_and_audio COMMAND vst3_smoke $<TARGET_FILE:Trackspace_VST3>)
        endif()
    endif()
endif()
