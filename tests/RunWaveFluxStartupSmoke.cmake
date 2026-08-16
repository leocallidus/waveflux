if(NOT DEFINED WAVEFLUX_BINARY OR WAVEFLUX_BINARY STREQUAL "")
    message(FATAL_ERROR "WAVEFLUX_BINARY is required")
endif()

if(NOT DEFINED WAVEFLUX_SMOKE_STATE_DIR OR WAVEFLUX_SMOKE_STATE_DIR STREQUAL "")
    set(WAVEFLUX_SMOKE_STATE_DIR "${CMAKE_CURRENT_BINARY_DIR}/waveflux-startup-smoke-state")
endif()
file(MAKE_DIRECTORY "${WAVEFLUX_SMOKE_STATE_DIR}")
file(MAKE_DIRECTORY "${WAVEFLUX_SMOKE_STATE_DIR}/config")
file(MAKE_DIRECTORY "${WAVEFLUX_SMOKE_STATE_DIR}/data")
file(MAKE_DIRECTORY "${WAVEFLUX_SMOKE_STATE_DIR}/runtime")

# A healthy GUI remains in its event loop and is stopped by this timeout. A
# QML registration/component failure exits immediately with a non-zero code.
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
        "QT_QPA_PLATFORM=offscreen"
        "QT_QUICK_BACKEND=software"
        "XDG_CONFIG_HOME=${WAVEFLUX_SMOKE_STATE_DIR}/config"
        "XDG_DATA_HOME=${WAVEFLUX_SMOKE_STATE_DIR}/data"
        "XDG_RUNTIME_DIR=${WAVEFLUX_SMOKE_STATE_DIR}/runtime"
        "${WAVEFLUX_BINARY}"
    RESULT_VARIABLE startup_result
    OUTPUT_VARIABLE startup_stdout
    ERROR_VARIABLE startup_stderr
    TIMEOUT 5
)

if("${startup_result}" MATCHES "timeout")
    message(STATUS "WaveFlux remained running with its QML component graph loaded")
    return()
endif()

message(FATAL_ERROR
    "WaveFlux exited before the startup smoke-test timeout (result: ${startup_result}).\n"
    "stdout:\n${startup_stdout}\n"
    "stderr:\n${startup_stderr}")
