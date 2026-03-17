#include <gtest/gtest.h>

#include <IO/WriteHelpers.h>
#include <string>
#include <vector>

using namespace DB;

TEST(WriteHelpersTest, ToStringWithFinalSeparatorTest) {
  {
    std::vector<std::string> v;
    EXPECT_EQ(toStringWithFinalSeparator(v, " or "), "");
  }
  {
    std::vector<std::string> v = {"AAA"};
    EXPECT_EQ(toStringWithFinalSeparator(v, " or "), "'AAA'");
  }
  {
    std::vector<std::string> v = {"AAA", "BBB"};
    EXPECT_EQ(toStringWithFinalSeparator(v, " or "), "'AAA' or 'BBB'");
  }
  {
    std::vector<std::string> v = {"AAA", "BBB", "CCC"};
    EXPECT_EQ(toStringWithFinalSeparator(v, " or "), "'AAA', 'BBB' or 'CCC'");
  }
  {
    std::vector<std::string> v = {"AAA", "BBB", "CCC", "DDD"};
    EXPECT_EQ(toStringWithFinalSeparator(v, " or "), "'AAA', 'BBB', 'CCC' or 'DDD'");
  }
}
