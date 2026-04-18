// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#ifdef _WIN32

#  include <gtest/gtest.h>
#  include <cstdint>
#  include <map>
#  include <nlohmann/json.hpp>
#  include <set>
#  include <string>
#  include <vector>

#  ifdef HAVE_MSGPACK
#    define _EVNT_SOURCE_
#    define EventWrite TestEventWrite
#    define EventWriteTransfer TestEventWriteTransfer
#  endif

#  define OPENTELEMETRY_ATTRIBUTE_TIMESTAMP_PREVIEW

#  include "opentelemetry/exporters/etw/etw_logger_exporter.h"
#  include "opentelemetry/exporters/etw/etw_tracer_exporter.h"
#  include "opentelemetry/sdk/trace/simple_processor.h"

#  ifdef HAVE_MSGPACK
#    undef _EVNT_SOURCE_
#    undef EventWrite
#    undef EventWriteTransfer
#  endif

using namespace OPENTELEMETRY_NAMESPACE;

using namespace opentelemetry::exporter::etw;

#  ifdef HAVE_MSGPACK
namespace
{
std::vector<std::uint8_t> g_last_msgpack_payload;
}

ULONG EVNTAPI TestEventWrite(REGHANDLE /*RegHandle*/,
                             PCEVENT_DESCRIPTOR /*EventDescriptor*/,
                             ULONG UserDataCount,
                             PEVENT_DATA_DESCRIPTOR UserData)
{
  g_last_msgpack_payload.clear();
  if (UserDataCount > 0)
  {
    auto *payload = reinterpret_cast<const std::uint8_t *>(UserData[0].Ptr);
    g_last_msgpack_payload.assign(payload, payload + UserData[0].Size);
  }
  return ERROR_SUCCESS;
}

ULONG EVNTAPI TestEventWriteTransfer(REGHANDLE RegHandle,
                                     PCEVENT_DESCRIPTOR EventDescriptor,
                                     LPCGUID /*ActivityId*/,
                                     LPCGUID /*RelatedActivityId*/,
                                     ULONG UserDataCount,
                                     PEVENT_DATA_DESCRIPTOR UserData)
{
  return TestEventWrite(RegHandle, EventDescriptor, UserDataCount, UserData);
}
#  endif

// The ETW provider ID is {4533CB59-77E2-54E9-E340-F0F0549058B7}
const char *kGlobalProviderName = "OpenTelemetry-ETW-TLD";

/**
 * @brief Logger test with name and unstructured text
 * {
 * "Timestamp": "2021-09-30T16:40:40.0820563-07:00",
 * "ProviderName": "OpenTelemetry-ETW-TLD",
 * "Id": 2,
 * "Message": null,
 * "ProcessId": 23180,
 * "Level": "Always",
 * "Keywords": "0x0000000000000000",
 * "EventName": "Log",
 * "ActivityID": null,
 * "RelatedActivityID": null,
 * "Payload": {
 *   "Name": "test",
 *   "SpanId": "0000000000000000",
 *   "Timestamp": "2021-09-30T23:40:40.066411500Z",
 *   "TraceId": "00000000000000000000000000000000",
 *   "_name": "Log",
 *   "body": "This is test message",
 *   "severityNumber": 5,
 *   "severityText": "DEBUG"
 * }
 * }
 */

TEST(ETWLogger, LoggerCheckWithBody)
{
  std::string providerName = kGlobalProviderName;  // supply unique instrumentation name here
  exporter::etw::LoggerProvider lp;

  const std::string schema_url{"https://opentelemetry.io/schemas/1.2.0"};
  auto logger        = lp.GetLogger(providerName, schema_url);
  Properties attribs = {{"attrib1", 1}, {"attrib2", 2}};
  EXPECT_NO_THROW(
      logger->EmitLogRecord(opentelemetry::logs::Severity::kDebug, "This is test log body"));
}

/**
 * @brief Logger Test with structured attributes
 *
 * Example Event for below test:
 * {
 *  "Timestamp": "2021-09-30T15:04:15.4227815-07:00",
 *  "ProviderName": "OpenTelemetry-ETW-TLD",
 * "Id": 1,
 * "Message": null,
 * "ProcessId": 33544,
 * "Level": "Always",
 * "Keywords": "0x0000000000000000",
 * "EventName": "Log",
 * "ActivityID": null,
 * "RelatedActivityID": null,
 * "Payload": {
 *  "Name": "test",
 *   "SpanId": "0000000000000000",
 *  "Timestamp": "2021-09-30T22:04:15.066411500Z",
 *   "TraceId": "00000000000000000000000000000000",
 *   "_name": "Log",
 *  "attrib1": 1,
 *   "attrib2": 2,
 *   "body": "",
 *   "severityNumber": 5,
 *   "severityText": "DEBUG"
 * }
 * }
 *
 */

TEST(ETWLogger, LoggerCheckWithAttributes)
{
  std::string providerName = kGlobalProviderName;  // supply unique instrumentation name here
  exporter::etw::LoggerProvider lp;

  const std::string schema_url{"https://opentelemetry.io/schemas/1.2.0"};
  auto logger = lp.GetLogger(providerName, schema_url);
  // Log attributes
  Properties attribs = {{"attrib1", 1}, {"attrib2", 2}};
  EXPECT_NO_THROW(logger->EmitLogRecord(opentelemetry::logs::Severity::kDebug,
                                        opentelemetry::common::MakeAttributes(attribs)));
}

/**
 * @brief Logger Test with structured attributes
 *
 * Example Event for below test:
 * {
 *  "Timestamp": "2024-06-02T15:04:15.4227815-07:00",
 *  "ProviderName": "OpenTelemetry-ETW-TLD",
 *  "Id": 1,
 *  "Message": null,
 *  "ProcessId": 37696,
 *  "Level": "Always",
 *  "Keywords": "0x0000000000000000",
 *  "EventName": "table1",
 *  "ActivityID": null,
 *  "RelatedActivityID": null,
 *  "Payload": {
 *   "SpanId": "0000000000000000",
 *   "Timestamp": "2021-09-30T22:04:15.066411500Z",
 *   "TraceId": "00000000000000000000000000000000",
 *   "_name": "table1",
 *   "attrib1": 1,
 *   "attrib2": "value2",
 *   "body": "This is a debug log body",
 *   "severityNumber": 5,
 *   "severityText": "DEBUG"
 *  }
 * }
 *
 */

TEST(ETWLogger, LoggerCheckWithTableNameMappings)
{
  std::string providerName = kGlobalProviderName;  // supply unique instrumentation name here
  std::map<std::string, std::string> tableNameMappings = {{"name1", "table1"}, {"name2", "table2"}};
  exporter::etw::TelemetryProviderOptions options      = {{"enableTableNameMappings", true},
                                                          {"tableNameMappings", tableNameMappings}};
  exporter::etw::LoggerProvider lp{options};

  auto logger = lp.GetLogger(providerName, "name1");

  // Log attributes
  Properties attribs = {{"attrib1", 1}, {"attrib2", "value2"}};

  EXPECT_NO_THROW(
      logger->Debug("This is a debug log body", opentelemetry::common::MakeAttributes(attribs)));
}

/**
 * @brief Logger Test with structured attributes
 *
 * Example Event for below test:
 * {
 *  "Timestamp": "2024-06-02T15:04:15.4227815-07:00",
 *  "ProviderName": "OpenTelemetry-ETW-TLD",
 *  "Id": 1,
 *  "Message": null,
 *  "ProcessId": 37696,
 *  "Level": "Always",
 *  "Keywords": "0x0000000000000000",
 *  "EventName": "table1",
 *  "ActivityID": null,
 *  "RelatedActivityID": null,
 *  "Payload": {
 *   "SpanId": "0000000000000000",
 *   "Timestamp": "2021-09-30T22:04:15.066411500Z",
 *   "TraceId": "00000000000000000000000000000000",
 *   "_name": "table1",
 *   "tiemstamp1": "2025-02-20T19:18:11.048166700Z",
 *   "attrib2": "value2",
 *   "body": "This is a debug log body",
 *   "severityNumber": 5,
 *   "severityText": "DEBUG"
 *  }
 * }
 *
 */

TEST(ETWLogger, LoggerCheckWithTimestampAttributes)
{
  std::string providerName = kGlobalProviderName;  // supply unique instrumentation name here
  std::set<std::string> timestampAttributes       = {{"timestamp1"}};
  exporter::etw::TelemetryProviderOptions options = {{"timestampAttributes", timestampAttributes}};
  exporter::etw::LoggerProvider lp{options};

  auto logger = lp.GetLogger(providerName, "name1");

  // Log attributes
  Properties attribs = {{"timestamp1", 133845526910481667ULL}, {"attrib2", "value2"}};

  EXPECT_NO_THROW(
      logger->Debug("This is a debug log body", opentelemetry::common::MakeAttributes(attribs)));
}

TEST(ETWLogger, LoggerProviderRecognizesMsgPackEncodingAliases)
{
  const char *encodings[] = {"MSGPACK", "MsgPack", "MessagePack"};
  for (const auto *encoding : encodings)
  {
    exporter::etw::TelemetryProviderOptions options = {{"encoding", std::string(encoding)}};
    exporter::etw::LoggerProvider lp{options};
    EXPECT_EQ(lp.config_.encoding, ETWProvider::EventFormat::ETW_MSGPACK);
  }
}

#  ifdef HAVE_MSGPACK
TEST(ETWLogger, LoggerEmitsMsgPackPayloadWhenConfigured)
{
  exporter::etw::TelemetryProviderOptions options = {{"encoding", std::string("MsgPack")}};
  exporter::etw::LoggerProvider lp{options};
  auto logger = lp.GetLogger("MsgPackLogger");

  g_last_msgpack_payload.clear();

  EXPECT_NO_THROW(
      logger->EmitLogRecord(opentelemetry::logs::Severity::kDebug, "This is msgpack logger body"));
  ASSERT_FALSE(g_last_msgpack_payload.empty());

  auto decoded              = nlohmann::json::from_msgpack(g_last_msgpack_payload);
  const auto &forwardName   = decoded.at(0);
  const auto &payloadObject = decoded.at(1).at(0).at(1);

  ASSERT_TRUE(forwardName.is_string());
  EXPECT_EQ(forwardName.get<std::string>(), ETW_VALUE_LOG);

  ASSERT_TRUE(payloadObject.at(ETW_FIELD_PAYLOAD_NAME).is_string());
  EXPECT_EQ(payloadObject.at(ETW_FIELD_PAYLOAD_NAME).get<std::string>(), "MsgPackLogger");

  ASSERT_TRUE(payloadObject.at(ETW_FIELD_LOG_BODY).is_string());
  EXPECT_EQ(payloadObject.at(ETW_FIELD_LOG_BODY).get<std::string>(), "This is msgpack logger body");

  ASSERT_TRUE(payloadObject.at(ETW_FIELD_LOG_SEVERITY_TEXT).is_string());
  EXPECT_EQ(payloadObject.at(ETW_FIELD_LOG_SEVERITY_TEXT).get<std::string>(), "DEBUG");

  ASSERT_TRUE(payloadObject.at(ETW_FIELD_LOG_SEVERITY_NUM).is_number_unsigned());
  EXPECT_EQ(payloadObject.at(ETW_FIELD_LOG_SEVERITY_NUM).get<uint32_t>(),
            static_cast<uint32_t>(opentelemetry::logs::Severity::kDebug));

  ASSERT_TRUE(payloadObject.at(ETW_FIELD_TIMESTAMP).is_string());
}
#  endif

/**
 * @brief Test that LogRecord created within an active span context
 *        inherits TraceId, SpanId, and TraceFlags from the current span.
 *
 * This test verifies the fix for issue #3830 where traceId and spanId
 * were incorrectly reported as all zeros.
 */
TEST(ETWLogger, LoggerInheritsTraceContextFromActiveSpan)
{
  std::string providerName = kGlobalProviderName;

  // Create tracer and logger providers
  exporter::etw::TracerProvider tp;
  exporter::etw::LoggerProvider lp;

  auto tracer = tp.GetTracer(providerName);
  auto logger = lp.GetLogger(providerName, "test");

  // Create a span and set it as the active span
  auto span  = tracer->StartSpan("TestSpan");
  auto scope = tracer->WithActiveSpan(span);

  // Get the span's trace context for verification
  auto span_context = span->GetContext();

  // Create a log record while the span is active
  auto log_record = logger->CreateLogRecord();
  ASSERT_NE(log_record, nullptr);

  // Cast to ETW LogRecord to access trace context
  auto *etw_record = static_cast<exporter::etw::LogRecord *>(log_record.get());

  // Verify TraceId matches the active span's TraceId
  EXPECT_EQ(etw_record->GetTraceId(), span_context.trace_id());

  // Verify SpanId matches the active span's SpanId
  EXPECT_EQ(etw_record->GetSpanId(), span_context.span_id());

  // Verify TraceFlags match
  EXPECT_EQ(etw_record->GetTraceFlags(), span_context.trace_flags());

  // Emit the log (should not throw)
  EXPECT_NO_THROW(logger->EmitLogRecord(std::move(log_record)));

  span->End();
}

/**
 * @brief Test that LogRecord created without an active span context
 *        has default (invalid/zero) TraceId and SpanId.
 */
TEST(ETWLogger, LoggerWithoutActiveSpanHasDefaultTraceContext)
{
  std::string providerName = kGlobalProviderName;
  exporter::etw::LoggerProvider lp;

  auto logger = lp.GetLogger(providerName, "test");

  // Create a log record without any active span
  auto log_record = logger->CreateLogRecord();
  ASSERT_NE(log_record, nullptr);

  auto *etw_record = static_cast<exporter::etw::LogRecord *>(log_record.get());

  // TraceId and SpanId should be invalid (all zeros) when no span is active
  EXPECT_FALSE(etw_record->GetTraceId().IsValid());
  EXPECT_FALSE(etw_record->GetSpanId().IsValid());

  EXPECT_NO_THROW(logger->EmitLogRecord(std::move(log_record)));
}

/**
 * @brief Test that LogRecord timestamp defaults to a valid time (not epoch).
 *
 * This test verifies the fix for issue #3830 where timestamp was
 * incorrectly reported as 1970-01-01 (epoch).
 */
TEST(ETWLogger, LoggerTimestampIsNotEpoch)
{
  std::string providerName = kGlobalProviderName;
  exporter::etw::LoggerProvider lp;

  auto logger = lp.GetLogger(providerName, "test");

  // Capture time before creating log record
  auto before = std::chrono::system_clock::now();

  auto log_record = logger->CreateLogRecord();
  ASSERT_NE(log_record, nullptr);

  // Capture time after creating log record
  auto after = std::chrono::system_clock::now();

  auto *etw_record = static_cast<exporter::etw::LogRecord *>(log_record.get());

  // Get the observed timestamp (which should be initialized to now())
  std::chrono::system_clock::time_point observed_ts = etw_record->GetObservedTimestamp();

  // Verify observed timestamp is not epoch (1970-01-01)
  EXPECT_GT(observed_ts.time_since_epoch().count(), 0);

  // Verify observed timestamp is within the expected range
  EXPECT_GE(observed_ts, before);
  EXPECT_LE(observed_ts, after);

  EXPECT_NO_THROW(logger->EmitLogRecord(std::move(log_record)));
}

#endif  // _WIN32
