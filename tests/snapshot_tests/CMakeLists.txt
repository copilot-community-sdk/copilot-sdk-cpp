# Snapshot conformance testing for copilot-sdk-cpp
#
# This builds the snapshot_replay executable used by snapshot_runner.py
# to validate SDK behavior against upstream snapshots.

find_package(Python3 REQUIRED COMPONENTS Interpreter)

add_executable(snapshot_replay
    snapshot_replay.cpp
)

target_link_libraries(snapshot_replay
    PRIVATE
        copilot_sdk_cpp
)

set_target_properties(snapshot_replay PROPERTIES FOLDER "Tests/Snapshots")

set(_monorepo_snapshot_dir "${CMAKE_CURRENT_SOURCE_DIR}/../../../copilot-sdk/test/snapshots")
set(_default_snapshot_dir "")
if(EXISTS "${_monorepo_snapshot_dir}")
    set(_default_snapshot_dir "${_monorepo_snapshot_dir}")
endif()

set(COPILOT_SDK_CPP_SNAPSHOT_DIR
    "${_default_snapshot_dir}"
    CACHE PATH
    "Path to upstream copilot-sdk snapshot directory (test/snapshots)"
)

if(COPILOT_SDK_CPP_SNAPSHOT_DIR AND EXISTS "${COPILOT_SDK_CPP_SNAPSHOT_DIR}")
    # Add a custom target to run snapshot tests
    add_custom_target(run_snapshot_tests
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/snapshot_runner.py
            --replay-exe $<TARGET_FILE:snapshot_replay>
            --snapshot-dir ${COPILOT_SDK_CPP_SNAPSHOT_DIR}
            --categories tools,session
        DEPENDS snapshot_replay
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Running snapshot conformance tests"
    )
else()
    message(STATUS "Snapshot tests enabled but COPILOT_SDK_CPP_SNAPSHOT_DIR is missing; skipping run_snapshot_tests target.")
endif()
