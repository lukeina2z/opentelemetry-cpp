// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstring>
#include <iostream>
#include <vector>

#include "opentelemetry/context/propagation/global_propagator.h"
#include "opentelemetry/context/propagation/text_map_propagator.h"
#include "opentelemetry/exporters/ostream/span_exporter_factory.h"
#include "opentelemetry/ext/http/client/http_client.h"
#include "opentelemetry/nostd/shared_ptr.h"
#include "opentelemetry/sdk/trace/exporter.h"
#include "opentelemetry/sdk/trace/processor.h"
#include "opentelemetry/sdk/trace/provider.h"

#include "opentelemetry/sdk/trace/batch_span_processor.h"
#include <opentelemetry/sdk/trace/simple_processor.h>
#include "opentelemetry/sdk/trace/simple_processor_factory.h"

#include "opentelemetry/sdk/trace/tracer_context.h"
#include "opentelemetry/sdk/trace/tracer_context_factory.h"
#include "opentelemetry/sdk/trace/tracer_provider_factory.h"
#include "opentelemetry/trace/propagation/http_trace_context.h"
#include "opentelemetry/trace/provider.h"

#include "opentelemetry/exporters/otlp/otlp_http.h"
#include "opentelemetry/exporters/otlp/otlp_http_client.h"
#include <opentelemetry/exporters/otlp/otlp_http_exporter.h>
#include "opentelemetry/exporters/otlp/otlp_http_exporter_factory.h"
#include "opentelemetry/exporters/otlp/otlp_http_exporter_options.h"

namespace http_example
{

template <typename T>
class HttpTextMapCarrier : public opentelemetry::context::propagation::TextMapCarrier
{
public:
  HttpTextMapCarrier(T &headers) : headers_(headers) {}
  HttpTextMapCarrier() = default;
  opentelemetry::nostd::string_view Get(
      opentelemetry::nostd::string_view key) const noexcept override
  {
    std::string key_to_compare = key.data();
    // Header's first letter seems to be  automatically capitaliazed by our test http-server, so
    // compare accordingly.
    if (key == opentelemetry::trace::propagation::kTraceParent)
    {
      key_to_compare = "Traceparent";
    }
    else if (key == opentelemetry::trace::propagation::kTraceState)
    {
      key_to_compare = "Tracestate";
    }
    auto it = headers_.find(key_to_compare);
    if (it != headers_.end())
    {
      return it->second;
    }
    return "";
  }

  void Set(opentelemetry::nostd::string_view key,
           opentelemetry::nostd::string_view value) noexcept override
  {
    headers_.insert(std::pair<std::string, std::string>(std::string(key), std::string(value)));
  }

  T headers_;
};

inline void InitTracer(const std::string& serviceName)
{
  const std::string ingestionSvc = "http://127.0.0.1:4318";

  opentelemetry::exporter::otlp::OtlpHttpExporterOptions opts;
  opts.url = ingestionSvc + "/v1/traces";
  opts.console_debug = true;
  
  opts.content_type  = opentelemetry::exporter::otlp::HttpRequestContentType::kJson;
  // opts.content_type  = opentelemetry::exporter::otlp::HttpRequestContentType::kBinary;

  auto otlp_http_exporter = std::make_unique<opentelemetry::exporter::otlp::OtlpHttpExporter>(opts);
  auto otlp_http_processor = std::make_unique<opentelemetry::sdk::trace::SimpleSpanProcessor>(
  std::move(otlp_http_exporter));

  auto ostream_exporter = opentelemetry::exporter::trace::OStreamSpanExporterFactory::Create();

  opentelemetry::sdk::trace::BatchSpanProcessorOptions options;
  options.max_queue_size = 10;
  options.schedule_delay_millis = std::chrono::milliseconds(3000);
  options.max_export_batch_size = 3;

  auto ostream_processor = std::make_unique<opentelemetry::sdk::trace::BatchSpanProcessor>(std::move(ostream_exporter), options);
  // auto ostream_processor = opentelemetry::sdk::trace::SimpleSpanProcessorFactory::Create(std::move(ostream_exporter));

  std::vector<std::unique_ptr<opentelemetry::sdk::trace::SpanProcessor>> processors;
  processors.push_back(std::move(ostream_processor));
  processors.push_back(std::move(otlp_http_processor));

  auto resource_attributes = opentelemetry::sdk::resource::ResourceAttributes{{"service.name", serviceName}};
  auto resource_ptr = opentelemetry::sdk::resource::Resource::Create(resource_attributes);

  // Default is an always-on sampler.
  std::unique_ptr<opentelemetry::sdk::trace::TracerContext> context =
      opentelemetry::sdk::trace::TracerContextFactory::Create(std::move(processors), resource_ptr);

  std::shared_ptr<opentelemetry::trace::TracerProvider> provider =
      opentelemetry::sdk::trace::TracerProviderFactory::Create(std::move(context));

  // Set the global trace provider
  opentelemetry::sdk::trace::Provider::SetTracerProvider(provider);

  // set global propagator
  opentelemetry::context::propagation::GlobalTextMapPropagator::SetGlobalPropagator(
      opentelemetry::nostd::shared_ptr<opentelemetry::context::propagation::TextMapPropagator>(
          new opentelemetry::trace::propagation::HttpTraceContext()));
}

inline void CleanupTracer()
{
  std::shared_ptr<opentelemetry::trace::TracerProvider> none;
  opentelemetry::sdk::trace::Provider::SetTracerProvider(none);
}

inline opentelemetry::nostd::shared_ptr<opentelemetry::trace::Tracer> get_tracer(
    const std::string &tracer_name)
{
  auto provider = opentelemetry::trace::Provider::GetTracerProvider();
  return provider->GetTracer(tracer_name);
}

}  // namespace http_example
