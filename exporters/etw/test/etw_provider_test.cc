// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#ifdef _WIN32

#  include <gtest/gtest.h>
#  include <cstdint>
#  include <nlohmann/json.hpp>
#  include <string>
#  include <vector>

#  ifdef HAVE_MSGPACK
#    define _EVNT_SOURCE_
#    define EventWrite TestEventWrite
#    define EventWriteTransfer TestEventWriteTransfer
#  endif

#  include "opentelemetry/exporters/etw/etw_provider.h"

#  ifdef HAVE_MSGPACK
#    undef _EVNT_SOURCE_
#    undef EventWrite
#    undef EventWriteTransfer
#  endif

using namespace OPENTELEMETRY_NAMESPACE;

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

TEST(ETWProvider, ProviderIsRegisteredSuccessfully)
{
  std::string providerName = "OpenTelemetry-ETW-Provider";
  static ETWProvider etw;
  auto handle = etw.open(providerName.c_str());

  bool registered = etw.is_registered(providerName);
  ASSERT_TRUE(registered);
  etw.close(handle);
}

TEST(ETWProvider, ProviderIsNotRegisteredSuccessfully)
{
  std::string providerName = "OpenTelemetry-ETW-Provider-NULL";
  static ETWProvider etw;

  bool registered = etw.is_registered(providerName);
  ASSERT_FALSE(registered);
}

TEST(ETWProvider, CheckOpenGUIDDataSuccessfully)
{
  std::string providerName = "OpenTelemetry-ETW-Provider";

  // get GUID from the handle returned
  static ETWProvider etw;
  auto handle = etw.open(providerName.c_str());

  utils::UUID uuid_handle(handle.providerGuid);
  auto guidStrHandle = uuid_handle.to_string();

  // get GUID from the providerName
  auto guid = utils::GetProviderGuid(providerName.c_str());
  utils::UUID uuid_name(guid);
  auto guidStrName = uuid_name.to_string();

  ASSERT_STREQ(guidStrHandle.c_str(), guidStrName.c_str());
  etw.close(handle);
}

TEST(ETWProvider, CheckCloseSuccess)
{
  std::string providerName = "OpenTelemetry-ETW-Provider";

  static ETWProvider etw;
  auto handle = etw.open(providerName.c_str(), ETWProvider::EventFormat::ETW_MANIFEST);
  auto result = etw.close(handle);
  ASSERT_NE(result, etw.STATUS_ERROR);
  ASSERT_FALSE(etw.is_registered(providerName));
}

TEST(ETWProvider, CheckCloseInfiniteLoop)
{
  std::string providerName1 = "Provider1";
  std::string providerName2 = "Provider2";

  static ETWProvider etw;
  auto handle1 = etw.open(providerName1.c_str());
  auto handle2 = etw.open(providerName2.c_str());

  // This should not hang
  auto result2 = etw.close(handle2);
  ASSERT_EQ(result2, etw.STATUS_OK);
  ASSERT_FALSE(etw.is_registered(providerName2));

  auto result1 = etw.close(handle1);
  ASSERT_EQ(result1, etw.STATUS_OK);
  ASSERT_FALSE(etw.is_registered(providerName1));
}

TEST(ETWProvider, CheckCloseRefCountUnderflow)
{
  std::string providerName = "OpenTelemetry-ETW-Provider-Underflow";
  static ETWProvider etw;
  auto handle = etw.open(providerName.c_str());
  auto result = etw.close(handle);
  ASSERT_EQ(result, etw.STATUS_OK);

  // Subsequent close should fail
  result = etw.close(handle);
  ASSERT_EQ(result, etw.STATUS_ERROR);
}

#  ifdef HAVE_MSGPACK
TEST(ETWProvider, MsgPackBoolPreservesBooleanType)
{
  ETWProvider etw;
  ETWProvider::Handle handle{};
  exporter::etw::Properties eventData = {{ETW_FIELD_NAME, "MyMsgPackEvent"},
                                         {"boolTrue", true},
                                         {"boolFalse", false}};

  handle.providerHandle = 1;
  g_last_msgpack_payload.clear();

  auto result = etw.writeMsgPack(handle, eventData);
  ASSERT_EQ(result, ERROR_SUCCESS);
  auto decoded = nlohmann::json::from_msgpack(g_last_msgpack_payload);
  const auto &payloadObject = decoded.at(1).at(0).at(1);

  ASSERT_TRUE(payloadObject.at("boolTrue").is_boolean());
  EXPECT_TRUE(payloadObject.at("boolTrue").get<bool>());
  ASSERT_TRUE(payloadObject.at("boolFalse").is_boolean());
  EXPECT_FALSE(payloadObject.at("boolFalse").get<bool>());
}

TEST(ETWProvider, MsgPackScalarPayloadRoundTrips)
{
  ETWProvider etw;
  ETWProvider::Handle handle{};
  exporter::etw::Properties eventData = {{ETW_FIELD_NAME, "ScalarMsgPackEvent"},
                                         {"boolTrue", true},
                                         {"int32Value", static_cast<int32_t>(-123)},
                                         {"int64Value", static_cast<int64_t>(-1234567890123LL)},
                                         {"uint32Value", static_cast<uint32_t>(123)},
                                         {"uint64Value", static_cast<uint64_t>(1234567890123ULL)},
                                         {"doubleValue", 3.25},
                                         {"stringValue", std::string("hello")},
                                         {"cstringValue", "world"}};

  handle.providerHandle = 1;
  g_last_msgpack_payload.clear();

  auto result = etw.writeMsgPack(handle, eventData);
  ASSERT_EQ(result, ERROR_SUCCESS);

  auto decoded                    = nlohmann::json::from_msgpack(g_last_msgpack_payload);
  const auto &forwardEventName    = decoded.at(0);
  const auto &payloadObject       = decoded.at(1).at(0).at(1);

  ASSERT_TRUE(forwardEventName.is_string());
  EXPECT_EQ(forwardEventName.get<std::string>(), "ScalarMsgPackEvent");
  EXPECT_FALSE(payloadObject.contains(ETW_FIELD_NAME));

  ASSERT_TRUE(payloadObject.at("boolTrue").is_boolean());
  EXPECT_TRUE(payloadObject.at("boolTrue").get<bool>());

  ASSERT_TRUE(payloadObject.at("int32Value").is_number_integer());
  EXPECT_EQ(payloadObject.at("int32Value").get<int32_t>(), -123);

  ASSERT_TRUE(payloadObject.at("int64Value").is_number_integer());
  EXPECT_EQ(payloadObject.at("int64Value").get<int64_t>(), -1234567890123LL);

  ASSERT_TRUE(payloadObject.at("uint32Value").is_number_unsigned());
  EXPECT_EQ(payloadObject.at("uint32Value").get<uint32_t>(), 123U);

  ASSERT_TRUE(payloadObject.at("uint64Value").is_number_unsigned());
  EXPECT_EQ(payloadObject.at("uint64Value").get<uint64_t>(), 1234567890123ULL);

  ASSERT_TRUE(payloadObject.at("doubleValue").is_number_float());
  EXPECT_DOUBLE_EQ(payloadObject.at("doubleValue").get<double>(), 3.25);

  ASSERT_TRUE(payloadObject.at("stringValue").is_string());
  EXPECT_EQ(payloadObject.at("stringValue").get<std::string>(), "hello");

  ASSERT_TRUE(payloadObject.at("cstringValue").is_string());
  EXPECT_EQ(payloadObject.at("cstringValue").get<std::string>(), "world");
}

TEST(ETWProvider, MsgPackAutoInjectsTimeField)
{
#  ifndef HAVE_FIELD_TIME
  GTEST_SKIP() << "HAVE_FIELD_TIME is disabled in this build configuration.";
#  endif

  ETWProvider etw;
  ETWProvider::Handle handle{};
  exporter::etw::Properties eventData = {{ETW_FIELD_NAME, "TimeInjectedEvent"}, {"value", 7}};

  handle.providerHandle = 1;
  g_last_msgpack_payload.clear();

  auto result = etw.writeMsgPack(handle, eventData);
  ASSERT_EQ(result, ERROR_SUCCESS);

  auto decoded              = nlohmann::json::from_msgpack(g_last_msgpack_payload);
  const auto &payloadObject = decoded.at(1).at(0).at(1);
  ASSERT_TRUE(payloadObject.contains(ETW_FIELD_TIME));

  const auto &timeField = payloadObject.at(ETW_FIELD_TIME);
  EXPECT_TRUE(timeField.is_number_integer() || timeField.is_number_unsigned());
  EXPECT_NE(timeField, 0);
}

TEST(ETWProvider, MsgPackInvalidNameFallsBackToNoName)
{
  ETWProvider etw;
  ETWProvider::Handle handle{};
  exporter::etw::Properties eventData = {{ETW_FIELD_NAME, true}, {"value", 11}};

  handle.providerHandle = 1;
  g_last_msgpack_payload.clear();

  auto result = etw.writeMsgPack(handle, eventData);
  ASSERT_EQ(result, ERROR_SUCCESS);

  auto decoded              = nlohmann::json::from_msgpack(g_last_msgpack_payload);
  const auto &forwardName   = decoded.at(0);
  const auto &payloadObject = decoded.at(1).at(0).at(1);

  ASSERT_TRUE(forwardName.is_string());
  EXPECT_EQ(forwardName.get<std::string>(), "NoName");
  EXPECT_FALSE(payloadObject.contains(ETW_FIELD_NAME));
  EXPECT_EQ(payloadObject.at("value").get<int32_t>(), 11);
}
#  endif

#endif
