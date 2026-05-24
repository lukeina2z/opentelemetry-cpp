// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "opentelemetry/context/propagation/global_propagator.h"
#include "opentelemetry/context/propagation/text_map_propagator.h"
#include "opentelemetry/exporters/ostream/span_exporter_factory.h"
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

#include "opentelemetry/logs/provider.h"
#include "opentelemetry/trace/provider.h"
#include "opentelemetry/trace/span.h"
#include "opentelemetry/exporters/geneva/geneva_logger_exporter.h"
#include "opentelemetry/exporters/geneva/geneva_tracer_exporter.h"

namespace LkLab
{
std::shared_ptr<opentelemetry::trace::TracerProvider> CreateOtlpTraceProvider(
    const std::string &serviceName)
{
  const std::string ingestionSvc = "http://127.0.0.1:4318";

  opentelemetry::exporter::otlp::OtlpHttpExporterOptions opts;
  opts.url           = ingestionSvc + "/v1/traces";
  opts.console_debug = true;

  opts.content_type = opentelemetry::exporter::otlp::HttpRequestContentType::kJson;
  // opts.content_type  = opentelemetry::exporter::otlp::HttpRequestContentType::kBinary;

  auto otlp_http_exporter = std::make_unique<opentelemetry::exporter::otlp::OtlpHttpExporter>(opts);
  auto otlp_http_processor = std::make_unique<opentelemetry::sdk::trace::SimpleSpanProcessor>(
      std::move(otlp_http_exporter));

  auto ostream_exporter = opentelemetry::exporter::trace::OStreamSpanExporterFactory::Create();

  opentelemetry::sdk::trace::BatchSpanProcessorOptions options;
  options.max_queue_size        = 10;
  options.schedule_delay_millis = std::chrono::milliseconds(3000);
  options.max_export_batch_size = 3;

  auto ostream_processor = std::make_unique<opentelemetry::sdk::trace::BatchSpanProcessor>(
      std::move(ostream_exporter), options);
  // auto ostream_processor =
  // opentelemetry::sdk::trace::SimpleSpanProcessorFactory::Create(std::move(ostream_exporter));

  std::vector<std::unique_ptr<opentelemetry::sdk::trace::SpanProcessor>> processors;
  processors.push_back(std::move(ostream_processor));
  processors.push_back(std::move(otlp_http_processor));

  auto resource_attributes =
      opentelemetry::sdk::resource::ResourceAttributes{{"service.name", serviceName}};
  auto resource_ptr = opentelemetry::sdk::resource::Resource::Create(resource_attributes);

  // Default is an always-on sampler.
  std::unique_ptr<opentelemetry::sdk::trace::TracerContext> context =
      opentelemetry::sdk::trace::TracerContextFactory::Create(std::move(processors), resource_ptr);

  std::shared_ptr<opentelemetry::trace::TracerProvider> provider =
      opentelemetry::sdk::trace::TracerProviderFactory::Create(std::move(context));

  return provider;
}

std::shared_ptr<opentelemetry::trace::TracerProvider> CreateEtwTraceProvider(const std::string &serviceName)
{
  std::shared_ptr<opentelemetry::trace::TracerProvider> spProvider =
      std::make_shared<opentelemetry::exporter::etw::TracerProvider>();
  
  return spProvider;
}

opentelemetry::nostd::shared_ptr<opentelemetry::trace::Tracer> GetTracer(std::string tracerName)
{
  auto provider = opentelemetry::trace::Provider::GetTracerProvider();
  return provider->GetTracer(tracerName);
}


class OpenTelemetryPipelineRuntime
{
public:
  OpenTelemetryPipelineRuntime(const std::string &serviceName) : m_serviceName(serviceName)
  {
    Initialize();
  }

protected:
  void Initialize()
  {
    m_provider = CreateOtlpTraceProvider(m_serviceName);
    // Set the global trace provider
    opentelemetry::sdk::trace::Provider::SetTracerProvider(m_provider);

    auto pHttpTraceContext = new opentelemetry::trace::propagation::HttpTraceContext();

    // set global propagator
    opentelemetry::context::propagation::GlobalTextMapPropagator::SetGlobalPropagator(
        opentelemetry::nostd::shared_ptr<opentelemetry::context::propagation::TextMapPropagator>(
            pHttpTraceContext));
  }

private:
  std::shared_ptr<opentelemetry::trace::TracerProvider> m_provider;
  std::string m_serviceName = "";
};

} // namespace LkLab


const char *kGlobalProviderName = "OpenTelemetry-ETW-TLD-Geneva-Example";
std::string providerName = kGlobalProviderName;

using L = std::vector<std::pair<trace_api::SpanContext, std::map<std::string, std::string>>>;

namespace
{
auto InitTracer()
{
  static opentelemetry::exporter::etw::TracerProvider tracer_provider;
  auto tracer = tracer_provider.GetTracer(providerName, "1.0");

  return tracer;
}
auto InitLogger()
{
  static opentelemetry::exporter::etw::LoggerProvider logger_provider;
  // auto logger = logger_provider.GetLogger(providerName, "1.0");
  auto logger = logger_provider.GetLogger(std::string(providerName) + "-Logger", "1.0");
  return logger;
}
}

void TestEtwExporter()
{
  auto traceProvider = LkLab::CreateEtwTraceProvider("LkLab-Foo-ETW");
  // Set the global trace provider
  opentelemetry::sdk::trace::Provider::SetTracerProvider(traceProvider);
  auto tracer = LkLab::GetTracer("InstrScope-Main-ETW");

  auto logger = InitLogger();

  auto fooEtw      = tracer->StartSpan("main");
  auto scopefooEtw = tracer->WithActiveSpan(fooEtw);

  {
    // Create Span with 1 SpanLink
    auto barEtw = tracer->StartSpan(
        "child", opentelemetry::common::MakeAttributes({{"key1", "value 1"}, {"key2", 1}}));

    barEtw->SetAttribute("attr_key1", 123);

    auto scopeBar = tracer->WithActiveSpan(barEtw);
    {
      // Create Span with 2 SpanLinks
      auto bazEtw = tracer->StartSpan(
          "grandchild", opentelemetry::common::MakeAttributes({{"key3", "value 3"}, {"key4", 2}}));

      bazEtw->SetAttribute("attr_key2", 456);
      bazEtw->End();
    }

    barEtw->End();
  }

  fooEtw->End();

  logger->Info("Hello World ETW!");
}

void TestOtlpExporter()
{

  auto traceProvider = LkLab::CreateOtlpTraceProvider("LkLab-OTLP-InsideOTelCpp");
  // Set the global trace provider
  opentelemetry::sdk::trace::Provider::SetTracerProvider(traceProvider);
  opentelemetry::nostd::shared_ptr<opentelemetry::trace::Tracer> tracer =
      LkLab::GetTracer("InstrScope-Main-Otlp");

  // auto logger = InitLogger();

  auto s1       = tracer->StartSpan("main");
  auto scopeFoo = tracer->WithActiveSpan(s1);

  {
    L link1 = {{s1->GetContext(), {}}};

    // Create Span with 1 SpanLink
    auto s2 = tracer->StartSpan(
        "child", opentelemetry::common::MakeAttributes({{"key1", "value 1"}, {"key2", 1}}), link1);

    s2->SetAttribute("attr_key1", 123);

    auto scopeBar = tracer->WithActiveSpan(s2);
    {
      L link2 = {{s1->GetContext(), {}}, {s2->GetContext(), {}}};

      // Create Span with 2 SpanLinks
      auto s3 = tracer->StartSpan(
          "grandchild", opentelemetry::common::MakeAttributes({{"key3", "value 3"}, {"key4", 2}}),
          link2);

      s3->SetAttribute("attr_key2", 456);

      s3->End();
    }

    s2->End();
  }

  s1->End();

  // logger->Info("Hello World!");
}

int main()
{
  TestEtwExporter();

  TestOtlpExporter();
   return 0;
}

int main_org()
{
  opentelemetry::nostd::shared_ptr<opentelemetry::trace::Tracer> tracerFoo = InitTracer();
  opentelemetry::nostd::shared_ptr<opentelemetry::trace::Tracer> tracer = InitTracer();
  auto logger = InitLogger();

  auto s1 = tracer->StartSpan("main");

  {
    L link1 = {{s1->GetContext(), {}}};

    // Create Span with 1 SpanLink
    auto s2 = tracer->StartSpan("child", opentelemetry::common::MakeAttributes({{"key1", "value 1"}, {"key2", 1}}), link1);

    s2->SetAttribute("attr_key1", 123);

    {
      L link2 = {{s1->GetContext(), {}}, {s2->GetContext(), {}}};

      // Create Span with 2 SpanLinks
      auto s3 = tracer->StartSpan("grandchild", opentelemetry::common::MakeAttributes({{"key3", "value 3"}, {"key4", 2}}), link2);

      s3->SetAttribute("attr_key2", 456);

      s3->End();
    }

    s2->End();
  }

  s1->End();

  logger->Info("Hello World!");

  return 0;
}