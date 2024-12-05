#include "ift/url_template.h"

#include "gtest/gtest.h"

namespace ift {

class URLTemplateTest : public ::testing::Test {
 protected:
  URLTemplateTest() {}
};

TEST_F(URLTemplateTest, PatchToUrl_NoFormatters) {
  std::string url("https://localhost/abc.patch");
  EXPECT_EQ(URLTemplate::PatchToUrl(url, 0), "https://localhost/abc.patch");
  EXPECT_EQ(URLTemplate::PatchToUrl(url, 5), "https://localhost/abc.patch");
}

TEST_F(URLTemplateTest, PatchToUrl_Basic) {
  // Test cases from: https://w3c.github.io/IFT/Overview.html#uri-templates
  EXPECT_EQ(URLTemplate::PatchToUrl("//foo.bar/{id}", 0), "//foo.bar/00");
  EXPECT_EQ(URLTemplate::PatchToUrl("//foo.bar/{id}", 123), "//foo.bar/FC");
  EXPECT_EQ(URLTemplate::PatchToUrl("//foo.bar{/d1,d2,id}", 478),
            "//foo.bar/0/F/07F0");
  EXPECT_EQ(URLTemplate::PatchToUrl("//foo.bar{/d1,d2,d3,id}", 123),
            "//foo.bar/C/F/_/FC");
}

}  // namespace ift
