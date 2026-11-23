#include "muduo20/base/Exception.h"

#include <gtest/gtest.h>
#include <source_location>
#include <string>
#include <string_view>

using namespace std::string_view_literals;

TEST(Exception, StoresMessageAndStack) {
    muduo20::Exception ex("oops"sv);
    EXPECT_STREQ(ex.what(), "oops");
    EXPECT_EQ(ex.whatView(), "oops");
    EXPECT_FALSE(ex.stackTraceView().empty());
}

TEST(Exception, LightweightConstructorSkipsStackTrace) {
    const muduo20::Exception ex("lite"sv, std::source_location::current());
    EXPECT_STREQ(ex.what(), "lite");
    EXPECT_TRUE(ex.stackTraceView().empty());
    EXPECT_FALSE(std::string_view{ex.fileName()}.empty());
    EXPECT_FALSE(std::string_view{ex.functionName()}.empty());
    EXPECT_GT(ex.line(), 0u);
}

TEST(Exception, ExplicitModeCanSkipOrCapture) {
    const muduo20::Exception skip("skip"sv,
                                  muduo20::Exception::StackTraceMode::kSkip);
    EXPECT_TRUE(skip.stackTraceView().empty());

    const muduo20::Exception capture(
        "capture"sv, muduo20::Exception::StackTraceMode::kCapture);
    EXPECT_FALSE(capture.stackTraceView().empty());
}

TEST(Exception, ThrowCatch) {
    try {
        throw muduo20::Exception("boom"sv);
    } catch (const muduo20::Exception &ex) {
        EXPECT_STREQ(ex.what(), "boom");
        EXPECT_FALSE(ex.stackTraceView().empty());
    }
}

TEST(Exception, LegacyStringCtor) {
    const std::string msg = "legacy-std::string";
    const muduo20::Exception ex(msg);
    EXPECT_STREQ(ex.what(), "legacy-std::string");
    EXPECT_FALSE(ex.stackTraceView().empty());
}
