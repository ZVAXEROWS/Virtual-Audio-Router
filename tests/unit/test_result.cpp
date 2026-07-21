// =============================================================================
// test_result.cpp — Unit tests for Result<T, VarError>
// =============================================================================

#include <gtest/gtest.h>
#include "var/Result.h"

using namespace var;

// ---------------------------------------------------------------------------
// Result<int, VarError>
// ---------------------------------------------------------------------------

TEST(ResultTest, OkHoldsValue) {
    auto r = Result<int, VarError>::ok(42);
    EXPECT_TRUE(r.isOk());
    EXPECT_FALSE(r.isErr());
    EXPECT_EQ(r.value(), 42);
}

TEST(ResultTest, ErrHoldsError) {
    VarError e{ ErrorCode::DeviceNotFound, "no device" };
    auto r = Result<int, VarError>::err(e);
    EXPECT_FALSE(r.isOk());
    EXPECT_TRUE(r.isErr());
    EXPECT_EQ(r.error().code, ErrorCode::DeviceNotFound);
    EXPECT_EQ(r.error().message, "no device");
}

TEST(ResultTest, BoolConversionOk) {
    auto r = Result<int, VarError>::ok(0);
    EXPECT_TRUE(static_cast<bool>(r));
}

TEST(ResultTest, BoolConversionErr) {
    auto r = Result<int, VarError>::err(VarError{ ErrorCode::Unknown });
    EXPECT_FALSE(static_cast<bool>(r));
}

TEST(ResultTest, ValueOrReturnsValueOnSuccess) {
    auto r = Result<int, VarError>::ok(99);
    EXPECT_EQ(r.valueOr(0), 99);
}

TEST(ResultTest, ValueOrReturnsDefaultOnError) {
    auto r = Result<int, VarError>::err(VarError{ ErrorCode::Unknown });
    EXPECT_EQ(r.valueOr(7), 7);
}

TEST(ResultTest, MapTransformsValue) {
    auto r = Result<int, VarError>::ok(10);
    auto mapped = r.map<std::string>([](int v) { return std::to_string(v); });
    EXPECT_TRUE(mapped.isOk());
    EXPECT_EQ(mapped.value(), "10");
}

TEST(ResultTest, MapPropagatesError) {
    auto r = Result<int, VarError>::err(VarError{ ErrorCode::Unknown, "fail" });
    auto mapped = r.map<std::string>([](int v) { return std::to_string(v); });
    EXPECT_TRUE(mapped.isErr());
    EXPECT_EQ(mapped.error().message, "fail");
}

TEST(ResultTest, PropagateConvertsType) {
    auto r = Result<int, VarError>::err(VarError{ ErrorCode::WasapiError, "wasapi" });
    auto propagated = r.propagate<double>();
    EXPECT_TRUE(propagated.isErr());
    EXPECT_EQ(propagated.error().code, ErrorCode::WasapiError);
}

TEST(ResultTest, AccessValueOnErrorThrows) {
    auto r = Result<int, VarError>::err(VarError{ ErrorCode::Unknown });
    EXPECT_THROW(r.value(), std::runtime_error);
}

TEST(ResultTest, AccessErrorOnOkThrows) {
    auto r = Result<int, VarError>::ok(1);
    EXPECT_THROW(r.error(), std::runtime_error);
}

// ---------------------------------------------------------------------------
// Result<void, VarError>
// ---------------------------------------------------------------------------

TEST(VoidResultTest, OkIsTrue) {
    auto r = VoidResult::ok();
    EXPECT_TRUE(r.isOk());
    EXPECT_FALSE(r.isErr());
}

TEST(VoidResultTest, ErrIsFalse) {
    auto r = VoidResult::err(VarError{ ErrorCode::ComInitFailed, "COM failed" });
    EXPECT_FALSE(r.isOk());
    EXPECT_TRUE(r.isErr());
    EXPECT_EQ(r.error().code, ErrorCode::ComInitFailed);
}

// ---------------------------------------------------------------------------
// VarError factory
// ---------------------------------------------------------------------------

TEST(VarErrorTest, FromHresult) {
    auto e = VarError::fromHresult(ErrorCode::WasapiError, 0x88890001u, "test");
    EXPECT_EQ(e.code, ErrorCode::WasapiError);
    EXPECT_EQ(e.hresult, 0x88890001u);
    EXPECT_NE(e.message.find("88890001"), std::string::npos);
}
