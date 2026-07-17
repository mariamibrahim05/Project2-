if (NOT DEFINED CACHE_EXE OR NOT DEFINED REFERENCE_CSV OR NOT DEFINED OUTPUT_CSV)
    message(FATAL_ERROR "CACHE_EXE, REFERENCE_CSV, and OUTPUT_CSV are required")
endif()

execute_process(
    COMMAND "${CACHE_EXE}" "${OUTPUT_CSV}"
    RESULT_VARIABLE simulator_result
    OUTPUT_VARIABLE simulator_output
    ERROR_VARIABLE simulator_error
)

if (NOT simulator_result EQUAL 0)
    message(FATAL_ERROR
        "Simulator failed with exit code ${simulator_result}\n${simulator_error}")
endif()

file(STRINGS "${OUTPUT_CSV}" csv_rows)
list(LENGTH csv_rows csv_line_count)
if (NOT csv_line_count EQUAL 31)
    message(FATAL_ERROR
        "Expected one header and 30 result rows, found ${csv_line_count} lines")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E compare_files
            "${REFERENCE_CSV}" "${OUTPUT_CSV}"
    RESULT_VARIABLE comparison_result
)

if (NOT comparison_result EQUAL 0)
    message(FATAL_ERROR "Fresh experiment results differ from results.csv")
endif()

message(STATUS "PASS: simulator completed all 30 runs")
message(STATUS "PASS: generated CSV has 30 data rows")
message(STATUS "PASS: fresh results exactly match results.csv")

