# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

if(WITH_HTTP_CLIENT_CURL)
  add_executable(http_client client.cc)
  add_executable(http_server server.cc)

target_link_libraries(
  http_client
  PRIVATE opentelemetry-cpp::trace opentelemetry-cpp::http_client_curl
          opentelemetry_http_client_curl opentelemetry_exporter_ostream_span
          opentelemetry_exporter_otlp_grpc
          opentelemetry_exporter_otlp_http
          opentelemetry-cpp::ostream_span_exporter CURL::libcurl)

target_link_libraries(
  http_server
  PRIVATE opentelemetry-cpp::trace opentelemetry-cpp::http_client_curl
          opentelemetry_exporter_otlp_grpc
          opentelemetry_exporter_otlp_http
          opentelemetry-cpp::ostream_span_exporter)
endif()
