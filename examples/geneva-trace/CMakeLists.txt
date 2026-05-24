# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

# add_executable(example_simple main.cc)
# target_link_libraries(example_simple PRIVATE common_foo_library)

# if(DEFINED OPENTELEMETRY_BUILD_DLL)
#   target_compile_definitions(example_simple
#                              PRIVATE OPENTELEMETRY_BUILD_IMPORT_DLL)
#   target_link_libraries(example_simple
#                         PRIVATE opentelemetry-cpp::opentelemetry_cpp)
# else()
#   target_link_libraries(
#     example_simple PRIVATE opentelemetry-cpp::trace
#                            opentelemetry-cpp::ostream_span_exporter)
# endif()

# if(BUILD_TESTING)
#   add_test(NAME examples.simple COMMAND "$<TARGET_FILE:example_simple>")
# endif()



add_executable(example_geneva-trace main.cc)

target_include_directories(example_geneva-trace
                           PRIVATE ${CMAKE_CURRENT_LIST_DIR}/include)

target_link_libraries(
     example_geneva-trace PRIVATE opentelemetry-cpp::trace
                    opentelemetry_exporter_etw
                    opentelemetry_exporter_otlp_http
                    opentelemetry-cpp::ostream_span_exporter)

if(WIN32)
  add_custom_command(
    TARGET example_geneva-trace
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_RUNTIME_DLLS:example_geneva-trace>"
            "$<TARGET_FILE_DIR:example_geneva-trace>"
    COMMAND_EXPAND_LISTS)
endif()
