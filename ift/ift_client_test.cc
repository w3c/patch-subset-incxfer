#include "ift/ift_client.h"

#include "gtest/gtest.h"

namespace ift {

class IFTClientTest : public ::testing::Test {
 protected:
  IFTClientTest() {
  }
};

TEST_F(IFTClientTest, PatchToUrl_NoFormatters) {
  std::string url("https://localhost/abc.patch");
  EXPECT_EQ(IFTClient::PatchToUrl(url, 0), "https://localhost/abc.patch");
  EXPECT_EQ(IFTClient::PatchToUrl(url, 5), "https://localhost/abc.patch");
}

TEST_F(IFTClientTest, PatchToUrl_Basic) {
  // Test cases from: https://w3c.github.io/IFT/Overview.html#uri-templates
  EXPECT_EQ(IFTClient::PatchToUrl("//foo.bar/{id}", 0), "//foo.bar/00");
  EXPECT_EQ(IFTClient::PatchToUrl("//foo.bar/{id}", 123), "//foo.bar/FC");
  EXPECT_EQ(IFTClient::PatchToUrl("//foo.bar{/d1,d2,id}", 478), "//foo.bar/0/F/07F0");
  EXPECT_EQ(IFTClient::PatchToUrl("//foo.bar{/d1,d2,d3,id}", 123), "//foo.bar/C/F/_/FC");
}

}  // namespace ift
