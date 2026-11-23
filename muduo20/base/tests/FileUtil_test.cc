#include "muduo20/base/FileUtil.h"

#include <gtest/gtest.h>

#include <cerrno>
#include <filesystem>
#include <string>
#include <string_view>

using namespace std::string_view_literals;

TEST(FileUtil, ReadFileBasicCases) {
    std::string content;
    int64_t size = 0;

    const int errCmdline = muduo20::FileUtil::readFile("/proc/self/cmdline"sv,
                                                       1024, &content, &size);
    EXPECT_EQ(errCmdline, 0);
    EXPECT_LE(content.size(), static_cast<size_t>(1024));

    const int errDir =
        muduo20::FileUtil::readFile("/proc/self"sv, 1024, &content, &size);
    EXPECT_EQ(errDir, EISDIR);

    const int errNotFound = muduo20::FileUtil::readFile(
        "/definitely/not/exist"sv, 1024, &content, &size);
    EXPECT_NE(errNotFound, 0);

    const int errZero =
        muduo20::FileUtil::readFile("/dev/zero"sv, 256, &content, &size);
    EXPECT_EQ(errZero, 0);
    EXPECT_EQ(content.size(), static_cast<size_t>(256));
}

TEST(FileUtil, AppendFileWritesBytes) {
    const std::string path = "/tmp/muduo20_fileutil_test.log";
    std::error_code ec;
    std::filesystem::remove(path, ec);
    muduo20::FileUtil::AppendFile file{std::string_view(path)};

    file.append("abc"sv);
    file.append("def"sv);
    file.append("ghi"sv);
    file.append("jkl"sv);
    file.flush();

    EXPECT_GE(file.writtenBytes(), 12);

    std::string content;
    int64_t size = 0;
    const int err = muduo20::FileUtil::readFile(std::string_view{path}, 1024,
                                                &content, &size);
    EXPECT_EQ(err, 0);
    EXPECT_GE(content.size(), static_cast<size_t>(12));
    std::filesystem::remove(path, ec);
}
