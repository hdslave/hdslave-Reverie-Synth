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
    BUNDLE_VERSION 0.3.7
    WINDOWS_FOLDER_VST3 TRUE
    ASSET_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/vst3")
set_target_properties(ReverieVST3 PROPERTIES PREFIX "")

# The adapter accepts any sample rate at setup and only discovers an unsupported
# rate when the host activates the synth. Report the engine's limit during setup.
set(adapter_target ReverieVST3-clap-wrapper-vst3-lib)
set(adapter_src "${reverie_clap_wrapper_SOURCE_DIR}/src")
target_compile_definitions(ReverieVST3 PRIVATE REVERIE_VST3_INPUT=1)

# Both adapter translation units must see the same private class layout.
set(header_src "${adapter_src}/detail/vst3/process.h")
file(READ "${header_src}" adapter_header)
set(event_vector "std::vector<clap_multi_event_t> _events;")
string(FIND "${adapter_header}" "${event_vector}" vector_offset)
if(vector_offset EQUAL -1)
    message(FATAL_ERROR "Review the VST3 input-event storage for this version of clap-wrapper")
endif()
string(REPLACE "${event_vector}" "reverie::Vst3EventBuffer<clap_multi_event_t> _events;" adapter_header "${adapter_header}")
string(REPLACE "#include \"../" "#include \"detail/" adapter_header "${adapter_header}")
file(CONFIGURE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/reverie_vst3_process.h"
    CONTENT "#include \"vst3_event_buffer.h\"
${adapter_header}" @ONLY)
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
string(REPLACE "\"detail/vst3/process.h\"" "\"reverie_vst3_process.h\"" processor "${processor}")
set(adapted_processor "${CMAKE_CURRENT_BINARY_DIR}/reverie_vst3_processor.cpp")
file(CONFIGURE OUTPUT "${adapted_processor}" CONTENT "#include \"sample_rate.h\"\n${processor}" @ONLY)

# Reverie does not consume per-note expression events. The adapter's lookup
# table for them otherwise grows on every note-on until it receives NOTE_END.
# Normal VST3 note events already carry their full address into the instrument.
set(events_src "${adapter_src}/detail/vst3/process.cpp")
file(READ "${events_src}" events)
set(track_note "addToActiveNotes(&n.note);")
string(FIND "${events}" "${track_note}" track_offset)
if(track_offset EQUAL -1)
    message(FATAL_ERROR "Review VST3 note-expression tracking for this version of clap-wrapper")
endif()
string(REPLACE "${track_note}" "" events "${events}")

# Turn each point of an instrument parameter queue into a CLAP event. Leave
# wrapper-owned preset selection and MIDI controller translation on their own
# paths. CLAP will consume the ordinary points at the samples chosen by the host.
set(last_parameter_point [=[          if (k->getPoint(nums - 1, offset, value) == kResultOk)
          {
            clap_multi_event_t n;
            n.param.header.type = CLAP_EVENT_PARAM_VALUE;]=])
set(all_parameter_points [=[          for (int32 point = 0; point < nums; ++point)
          {
            if (k->getPoint(point, offset, value) != kResultOk) continue;
            clap_multi_event_t n;
            n.param.header.type = CLAP_EVENT_PARAM_VALUE;]=])
string(FIND "${events}" "${last_parameter_point}" automation_offset)
if(automation_offset EQUAL -1)
    message(FATAL_ERROR "Review the VST3 parameter queue conversion for this version of clap-wrapper")
endif()
string(REPLACE "${last_parameter_point}" "${all_parameter_points}" events "${events}")

# Retain every ordinary point and the adapter's existing note/MIDI allowance.
# Sorted offsets address stable event bodies until the whole process call returns.
set(old_event_reserve [=[  _events.reserve(8192);
  _eventindices.clear();
  _eventindices.reserve(_events.capacity());]=])
set(new_event_reserve [=[  const size_t points = size_t(numSamples) * (ext_params ? ext_params->count(plugin) : 0);
  _events.prepare(8192, points);
  _eventindices.clear();
  _eventindices.reserve(8192 + points);]=])
string(FIND "${events}" "${old_event_reserve}" reserve_offset)
if(reserve_offset EQUAL -1)
    message(FATAL_ERROR "Review the VST3 event reservation for this version of clap-wrapper")
endif()
string(REPLACE "${old_event_reserve}" "${new_event_reserve}" events "${events}")
string(REGEX REPLACE "_eventindices\\.push_back\\(_events\\.size\\(\\)\\);[ \t\r\n]*_events\\.push_back\\(n\\);"
    "_eventindices.push_back(_events.append(n));" events "${events}")
string(REPLACE "self->_events.size()" "self->_eventindices.size()" events "${events}")
string(REPLACE "&(self->_events[realindex].header)" "self->_events.at(realindex)" events "${events}")
string(REPLACE "_events[a].header.time" "_events.at(a)->time" events "${events}")
string(REPLACE "_events[b].header.time" "_events.at(b)->time" events "${events}")
# Preserve upstream relative includes without putting its process.h ahead of the Windows SDK.
string(REPLACE "#include \"" "#include \"detail/vst3/" events "${events}")
string(REPLACE "\"detail/vst3/process.h\"" "\"reverie_vst3_process.h\"" events "${events}")
set(adapted_events "${CMAKE_CURRENT_BINARY_DIR}/reverie_vst3_events.cpp")
file(CONFIGURE OUTPUT "${adapted_events}" CONTENT "${events}" @ONLY)
get_target_property(adapter_sources ${adapter_target} SOURCES)
list(REMOVE_ITEM adapter_sources "${processor_src}" "${events_src}")
set_property(TARGET ${adapter_target} PROPERTY SOURCES ${adapter_sources} "${adapted_processor}" "${adapted_events}")
target_include_directories(${adapter_target} PRIVATE "${adapter_src}" "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake" "${DESKTOP_DIR}")

add_executable(vst3_automation_test "${CMAKE_CURRENT_SOURCE_DIR}/tests/vst3_automation.cpp")
target_include_directories(vst3_automation_test PRIVATE "${VST3_SDK_ROOT}")
if(APPLE)
    target_link_libraries(vst3_automation_test PRIVATE "-framework CoreFoundation")
endif()
