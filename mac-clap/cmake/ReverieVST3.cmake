include(FetchContent)

# The adapter uses CLAP 1.2.10 headers. The instrument uses its vendored ABI.
FetchContent_Declare(reverie_adapter_clap
    GIT_REPOSITORY https://github.com/free-audio/clap.git
    GIT_TAG 195b42a004144fab0b3cf95e9c067187d15365b7)
FetchContent_MakeAvailable(reverie_adapter_clap)

# clap-wrapper builds the SDK sources it uses. Skip the SDK's example projects.
FetchContent_Declare(reverie_vst_sdk
    GIT_REPOSITORY https://github.com/steinbergmedia/vst3sdk.git
    GIT_TAG 9fad9770f2ae8542ab1a548a68c1ad1ac690abe0
    GIT_SUBMODULES base public.sdk pluginterfaces cmake
    SOURCE_SUBDIR unused)
FetchContent_MakeAvailable(reverie_vst_sdk)
set(VST3_SDK_ROOT "${reverie_vst_sdk_SOURCE_DIR}")

FetchContent_Declare(reverie_clap_wrapper
    GIT_REPOSITORY https://github.com/free-audio/clap-wrapper.git
    GIT_TAG c3a2531a5e66572b4246fc64271e2103d0de37bb)
FetchContent_MakeAvailable(reverie_clap_wrapper)
target_compile_definitions(clap-wrapper-shared-detail PRIVATE STATICALLY_LINKED_CLAP_ENTRY=1)

add_reverie_editor(ReverieVST3 HdslavePanelView_reverie_vst3)
target_add_vst3_wrapper(TARGET ReverieVST3 OUTPUT_NAME Reverie
    BUNDLE_IDENTIFIER com.hdslave.desktop.reverie.vst3
    BUNDLE_VERSION 0.2.0
    WINDOWS_FOLDER_VST3 TRUE
    ASSET_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/vst3")
set_target_properties(ReverieVST3 PROPERTIES PREFIX "")

# The adapter accepts any sample rate at setup and only discovers an unsupported
# rate when the host activates the synth. Report the engine's limit during setup.
set(adapter_target ReverieVST3-clap-wrapper-vst3-lib)
set(adapter_src "${reverie_clap_wrapper_SOURCE_DIR}/src")
set(processor_src "${adapter_src}/wrapasvst3.cpp")
file(READ "${processor_src}" processor)
set(old_check "if (newSetup.symbolicSampleSize != Vst::kSample32)")
string(FIND "${processor}" "${old_check}" check_offset)
if(check_offset EQUAL -1)
    message(FATAL_ERROR "Review the VST3 sample-rate check for this version of clap-wrapper")
endif()
string(REPLACE "${old_check}"
    "if (!reverie::supportsSampleRate(newSetup.sampleRate) || newSetup.symbolicSampleSize != Vst::kSample32)"
    processor "${processor}")
set(adapted_processor "${CMAKE_CURRENT_BINARY_DIR}/reverie_vst3_processor.cpp")
file(CONFIGURE OUTPUT "${adapted_processor}" CONTENT "#include \"sample_rate.h\"\n${processor}" @ONLY)
get_target_property(adapter_sources ${adapter_target} SOURCES)
list(REMOVE_ITEM adapter_sources "${processor_src}")
set_property(TARGET ${adapter_target} PROPERTY SOURCES ${adapter_sources} "${adapted_processor}")
target_include_directories(${adapter_target} PRIVATE "${adapter_src}" "${CMAKE_CURRENT_SOURCE_DIR}/src")
