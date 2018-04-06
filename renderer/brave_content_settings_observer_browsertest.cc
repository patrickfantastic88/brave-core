/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/path_service.h"
#include "brave/common/brave_paths.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "net/dns/mock_host_resolver.h"

const char kIframeID[] = "test";
const char kPointInPathScript[] =
  "var canvas = document.createElement('canvas');"
  "var ctx = canvas.getContext('2d');"
  "ctx.rect(10, 10, 100, 100);"
  "ctx.stroke();"
  "domAutomationController.send(ctx.isPointInPath(10, 10));";

#define COOKIE_STRING "test=hi"
const char kSetAndGetCookieScript[] =
    "document.cookie = '"
    COOKIE_STRING
    "'; domAutomationController.send(document.cookie);";
const char kGetReferrerScript[] =
    "domAutomationController.send(document.referrer);";

class BraveContentSettingsObserverBrowserTest : public InProcessBrowserTest {
  public:
    void SetUpOnMainThread() override {
      InProcessBrowserTest::SetUpOnMainThread();
      host_resolver()->AddRule("*", "127.0.0.1");
      content::SetupCrossSiteRedirector(embedded_test_server());

      brave::RegisterPathProvider();
      base::FilePath test_data_dir;
      PathService::Get(brave::DIR_TEST_DATA, &test_data_dir);
      embedded_test_server()->ServeFilesFromDirectory(test_data_dir);

      ASSERT_TRUE(embedded_test_server()->Start());

      url_ = embedded_test_server()->GetURL("a.com", "/iframe.html");
      iframe_url_ = embedded_test_server()->GetURL("b.com", "/simple.html");
      top_level_page_pattern_ = ContentSettingsPattern::FromString("http://a.com/*");
      first_party_pattern_ = ContentSettingsPattern::FromString("https://firstParty/*");
    }

    const GURL& url() { return url_; }
    const GURL& iframe_url() { return iframe_url_; }

    const ContentSettingsPattern& top_level_page_pattern() {
      return top_level_page_pattern_;
    }

    const ContentSettingsPattern& first_party_pattern() {
      return first_party_pattern_;
    }

    const ContentSettingsPattern& empty_pattern() {
      return empty_pattern_;
    }
  private:
    GURL url_;
    GURL iframe_url_;
    ContentSettingsPattern top_level_page_pattern_;
    ContentSettingsPattern first_party_pattern_;
    ContentSettingsPattern empty_pattern_;
};

IN_PROC_BROWSER_TEST_F(BraveContentSettingsObserverBrowserTest, BlockThirdPartyFPByDefault) {
  HostContentSettingsMap* content_settings =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  ContentSettingsForOneType fp_settings;
  content_settings->GetSettingsForOneType(
      CONTENT_SETTINGS_TYPE_PLUGINS, "fingerprinting", &fp_settings);
  EXPECT_EQ(fp_settings.size(), 1u) <<
      "There should be one default fingerprinting rule.";
  EXPECT_EQ(fp_settings[0].primary_pattern, ContentSettingsPattern::Wildcard()) <<
      "Primary pattern of default fingerprinting rule should be wildcard.";
  EXPECT_EQ(fp_settings[0].secondary_pattern, first_party_pattern()) <<
      "Secondary pattern of default fingerprinting rule should be the special "
      "first party pattern we defined.";

  ui_test_utils::NavigateToURL(browser(), url());
  content::WebContents* contents =
    browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(contents->GetAllFrames().size(), 2u) <<
    "Two frames (main + iframe) should be created.";

  content::RenderFrameHost* main_frame = contents->GetMainFrame();
  content::RenderFrameHost* child_frame =
    ChildFrameAt(contents->GetMainFrame(), 0);
  EXPECT_EQ(main_frame->GetLastCommittedURL(), url());

  bool isPointInPath;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(contents, kPointInPathScript, &isPointInPath));
  EXPECT_TRUE(isPointInPath);

  EXPECT_TRUE(NavigateIframeToURL(contents, kIframeID, iframe_url()));
  EXPECT_EQ(child_frame->GetLastCommittedURL(), iframe_url());
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      child_frame, kPointInPathScript, &isPointInPath));
  EXPECT_FALSE(isPointInPath);
}

IN_PROC_BROWSER_TEST_F(BraveContentSettingsObserverBrowserTest, BlockFP) {
  HostContentSettingsMap* content_settings =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  content_settings->SetContentSettingCustomScope(top_level_page_pattern(),
      ContentSettingsPattern::Wildcard(), CONTENT_SETTINGS_TYPE_PLUGINS,
      "fingerprinting", CONTENT_SETTING_BLOCK);
  content_settings->SetContentSettingCustomScope(top_level_page_pattern(),
      first_party_pattern(), CONTENT_SETTINGS_TYPE_PLUGINS,
      "fingerprinting", CONTENT_SETTING_BLOCK);

  ContentSettingsForOneType fp_settings;
  content_settings->GetSettingsForOneType(
      CONTENT_SETTINGS_TYPE_PLUGINS, "fingerprinting", &fp_settings);
  EXPECT_EQ(fp_settings.size(), 3u);

  ui_test_utils::NavigateToURL(browser(), url());
  content::WebContents* contents =
    browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(contents->GetAllFrames().size(), 2u) <<
    "Two frames (main + iframe) should be created.";

  content::RenderFrameHost* main_frame = contents->GetMainFrame();
  content::RenderFrameHost* child_frame =
    ChildFrameAt(contents->GetMainFrame(), 0);
  EXPECT_EQ(main_frame->GetLastCommittedURL(), url());

  bool isPointInPath;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(contents, kPointInPathScript, &isPointInPath));
  EXPECT_FALSE(isPointInPath);

  EXPECT_TRUE(NavigateIframeToURL(contents, kIframeID, iframe_url()));
  EXPECT_EQ(child_frame->GetLastCommittedURL(), iframe_url());
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      child_frame, kPointInPathScript, &isPointInPath));
  EXPECT_FALSE(isPointInPath);
}

IN_PROC_BROWSER_TEST_F(BraveContentSettingsObserverBrowserTest, AllowFP) {
  HostContentSettingsMap* content_settings =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  content_settings->SetContentSettingCustomScope(top_level_page_pattern(),
      ContentSettingsPattern::Wildcard(), CONTENT_SETTINGS_TYPE_PLUGINS,
      "fingerprinting", CONTENT_SETTING_ALLOW);
  content_settings->SetContentSettingCustomScope(top_level_page_pattern(),
      first_party_pattern(), CONTENT_SETTINGS_TYPE_PLUGINS,
      "fingerprinting", CONTENT_SETTING_ALLOW);

  ContentSettingsForOneType fp_settings;
  content_settings->GetSettingsForOneType(
      CONTENT_SETTINGS_TYPE_PLUGINS, "fingerprinting", &fp_settings);
  EXPECT_EQ(fp_settings.size(), 3u);

  ui_test_utils::NavigateToURL(browser(), url());
  content::WebContents* contents =
    browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(contents->GetAllFrames().size(), 2u) <<
    "Two frames (main + iframe) should be created.";

  content::RenderFrameHost* main_frame = contents->GetMainFrame();
  content::RenderFrameHost* child_frame =
    ChildFrameAt(contents->GetMainFrame(), 0);
  EXPECT_EQ(main_frame->GetLastCommittedURL(), url());

  bool isPointInPath;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(contents, kPointInPathScript, &isPointInPath));
  EXPECT_TRUE(isPointInPath);

  EXPECT_TRUE(NavigateIframeToURL(contents, kIframeID, iframe_url()));
  EXPECT_EQ(child_frame->GetLastCommittedURL(), iframe_url());
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      child_frame, kPointInPathScript, &isPointInPath));
  EXPECT_TRUE(isPointInPath);
}

IN_PROC_BROWSER_TEST_F(BraveContentSettingsObserverBrowserTest, BlockThirdPartyFP) {
  HostContentSettingsMap* content_settings =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  content_settings->SetContentSettingCustomScope(top_level_page_pattern(),
      ContentSettingsPattern::Wildcard(), CONTENT_SETTINGS_TYPE_PLUGINS,
      "fingerprinting", CONTENT_SETTING_BLOCK);
  content_settings->SetContentSettingCustomScope(top_level_page_pattern(),
      first_party_pattern(), CONTENT_SETTINGS_TYPE_PLUGINS,
      "fingerprinting", CONTENT_SETTING_ALLOW);

  ContentSettingsForOneType fp_settings;
  content_settings->GetSettingsForOneType(
      CONTENT_SETTINGS_TYPE_PLUGINS, "fingerprinting", &fp_settings);
  EXPECT_EQ(fp_settings.size(), 3u);

  ui_test_utils::NavigateToURL(browser(), url());
  content::WebContents* contents =
    browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(contents->GetAllFrames().size(), 2u) <<
    "Two frames (main + iframe) should be created.";

  content::RenderFrameHost* main_frame = contents->GetMainFrame();
  content::RenderFrameHost* child_frame =
    ChildFrameAt(contents->GetMainFrame(), 0);
  EXPECT_EQ(main_frame->GetLastCommittedURL(), url());

  bool isPointInPath;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(contents, kPointInPathScript, &isPointInPath));
  EXPECT_TRUE(isPointInPath);

  EXPECT_TRUE(NavigateIframeToURL(contents, kIframeID, iframe_url()));
  EXPECT_EQ(child_frame->GetLastCommittedURL(), iframe_url());
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      child_frame, kPointInPathScript, &isPointInPath));
  EXPECT_FALSE(isPointInPath);
}

IN_PROC_BROWSER_TEST_F(BraveContentSettingsObserverBrowserTest, BlockThirdPartyReferrerByDefault) {
  HostContentSettingsMap* content_settings =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  ContentSettingsForOneType settings;
  content_settings->GetSettingsForOneType(
      CONTENT_SETTINGS_TYPE_PLUGINS, "referers", &settings);
  EXPECT_EQ(settings.size(), 1u) <<
      "There should be one default referrer rule.";
  EXPECT_EQ(settings[0].primary_pattern, ContentSettingsPattern::Wildcard()) <<
      "Primary pattern of default referrer rule should be wildcard.";
  EXPECT_EQ(settings[0].secondary_pattern, ContentSettingsPattern::Wildcard()) <<
      "secondary pattern of default referrer rule should be wildcard.";

  ui_test_utils::NavigateToURL(browser(), url());
  content::WebContents* contents =
    browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(contents->GetAllFrames().size(), 2u) <<
    "Two frames (main + iframe) should be created.";

  content::RenderFrameHost* main_frame = contents->GetMainFrame();
  content::RenderFrameHost* child_frame =
    ChildFrameAt(contents->GetMainFrame(), 0);
  EXPECT_EQ(main_frame->GetLastCommittedURL(), url());

  std::string value;
  ASSERT_TRUE(ExecuteScriptAndExtractString(contents, kGetReferrerScript, &value));
  EXPECT_STREQ(value.c_str(), "");

  ASSERT_TRUE(NavigateIframeToURL(contents, kIframeID, iframe_url()));
  ASSERT_EQ(child_frame->GetLastCommittedURL(), iframe_url());
  ASSERT_TRUE(ExecuteScriptAndExtractString(child_frame, kGetReferrerScript, &value));
  EXPECT_STREQ(value.c_str(), iframe_url().GetOrigin().spec().c_str());
}

IN_PROC_BROWSER_TEST_F(BraveContentSettingsObserverBrowserTest, BlockReferrer) {
  HostContentSettingsMap* content_settings =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  content_settings->SetContentSettingCustomScope(top_level_page_pattern(),
      empty_pattern(), CONTENT_SETTINGS_TYPE_PLUGINS,
      "referers", CONTENT_SETTING_BLOCK);

  ContentSettingsForOneType settings;
  content_settings->GetSettingsForOneType(
      CONTENT_SETTINGS_TYPE_PLUGINS, "referers", &settings);
  EXPECT_EQ(settings.size(), 2u);

  ui_test_utils::NavigateToURL(browser(), url());
  content::WebContents* contents =
    browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(contents->GetAllFrames().size(), 2u) <<
    "Two frames (main + iframe) should be created.";

  content::RenderFrameHost* main_frame = contents->GetMainFrame();
  content::RenderFrameHost* child_frame =
    ChildFrameAt(contents->GetMainFrame(), 0);
  EXPECT_EQ(main_frame->GetLastCommittedURL(), url());

  std::string value;
  ASSERT_TRUE(ExecuteScriptAndExtractString(contents, kGetReferrerScript, &value));
  EXPECT_STREQ(value.c_str(), "");

  ASSERT_TRUE(NavigateIframeToURL(contents, kIframeID, iframe_url()));
  ASSERT_EQ(child_frame->GetLastCommittedURL(), iframe_url());
  ASSERT_TRUE(ExecuteScriptAndExtractString(child_frame, kGetReferrerScript, &value));
  EXPECT_STREQ(value.c_str(), iframe_url().GetOrigin().spec().c_str());
}

IN_PROC_BROWSER_TEST_F(BraveContentSettingsObserverBrowserTest, AllowReferrer) {
  HostContentSettingsMap* content_settings =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  content_settings->SetContentSettingCustomScope(top_level_page_pattern(),
      ContentSettingsPattern::Wildcard(), CONTENT_SETTINGS_TYPE_PLUGINS,
      "referers", CONTENT_SETTING_ALLOW);

  ContentSettingsForOneType settings;
  content_settings->GetSettingsForOneType(
      CONTENT_SETTINGS_TYPE_PLUGINS, "referers", &settings);
  EXPECT_EQ(settings.size(), 2u);

  ui_test_utils::NavigateToURL(browser(), url());
  content::WebContents* contents =
    browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(contents->GetAllFrames().size(), 2u) <<
    "Two frames (main + iframe) should be created.";

  content::RenderFrameHost* main_frame = contents->GetMainFrame();
  content::RenderFrameHost* child_frame =
    ChildFrameAt(contents->GetMainFrame(), 0);
  EXPECT_EQ(main_frame->GetLastCommittedURL(), url());

  std::string value;
  ASSERT_TRUE(ExecuteScriptAndExtractString(contents, kGetReferrerScript, &value));
  EXPECT_STREQ(value.c_str(), "");

  ASSERT_TRUE(NavigateIframeToURL(contents, kIframeID, iframe_url()));
  ASSERT_EQ(child_frame->GetLastCommittedURL(), iframe_url());
  ASSERT_TRUE(ExecuteScriptAndExtractString(child_frame, kGetReferrerScript, &value));
  EXPECT_STREQ(value.c_str(), url().spec().c_str());
}

IN_PROC_BROWSER_TEST_F(BraveContentSettingsObserverBrowserTest, BlockThirdPartyCookieByDefault) {
  ui_test_utils::NavigateToURL(browser(), url());
  content::WebContents* contents =
    browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(contents->GetAllFrames().size(), 2u) <<
    "Two frames (main + iframe) should be created.";

  content::RenderFrameHost* main_frame = contents->GetMainFrame();
  content::RenderFrameHost* child_frame =
    ChildFrameAt(contents->GetMainFrame(), 0);
  EXPECT_EQ(main_frame->GetLastCommittedURL(), url());

  std::string value;
  ASSERT_TRUE(ExecuteScriptAndExtractString(contents, kSetAndGetCookieScript, &value));
  EXPECT_STREQ(value.c_str(), COOKIE_STRING);

  ASSERT_TRUE(NavigateIframeToURL(contents, kIframeID, iframe_url()));
  ASSERT_EQ(child_frame->GetLastCommittedURL(), iframe_url());
  ASSERT_TRUE(ExecuteScriptAndExtractString(child_frame, kSetAndGetCookieScript, &value));
  EXPECT_STREQ(value.c_str(), "");
}

IN_PROC_BROWSER_TEST_F(BraveContentSettingsObserverBrowserTest, BlockCookies) {
  HostContentSettingsMap* content_settings =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  content_settings->SetContentSettingCustomScope(top_level_page_pattern(),
      ContentSettingsPattern::Wildcard(), CONTENT_SETTINGS_TYPE_COOKIES,
      std::string(), CONTENT_SETTING_BLOCK);

  ui_test_utils::NavigateToURL(browser(), url());
  content::WebContents* contents =
    browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(contents->GetAllFrames().size(), 2u) <<
    "Two frames (main + iframe) should be created.";

  content::RenderFrameHost* main_frame = contents->GetMainFrame();
  content::RenderFrameHost* child_frame =
    ChildFrameAt(contents->GetMainFrame(), 0);
  EXPECT_EQ(main_frame->GetLastCommittedURL(), url());

  std::string value;
  ASSERT_TRUE(ExecuteScriptAndExtractString(contents, kSetAndGetCookieScript, &value));
  EXPECT_STREQ(value.c_str(), "");

  ASSERT_TRUE(NavigateIframeToURL(contents, kIframeID, iframe_url()));
  ASSERT_EQ(child_frame->GetLastCommittedURL(), iframe_url());
  ASSERT_TRUE(ExecuteScriptAndExtractString(child_frame, kSetAndGetCookieScript, &value));
  EXPECT_STREQ(value.c_str(), "");
}

IN_PROC_BROWSER_TEST_F(BraveContentSettingsObserverBrowserTest, AllowCookies) {
  HostContentSettingsMap* content_settings =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  auto pattern2 = ContentSettingsPattern::FromString("http://a.com/*");
  content_settings->SetContentSettingCustomScope(
      ContentSettingsPattern::Wildcard(),
      top_level_page_pattern(),
      CONTENT_SETTINGS_TYPE_COOKIES,
      std::string(), CONTENT_SETTING_ALLOW);
  content_settings->SetContentSettingCustomScope(
      ContentSettingsPattern::Wildcard(),
      pattern2,
      CONTENT_SETTINGS_TYPE_COOKIES,
      std::string(), CONTENT_SETTING_ALLOW);

  ui_test_utils::NavigateToURL(browser(), url());
  content::WebContents* contents =
    browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(contents->GetAllFrames().size(), 2u) <<
    "Two frames (main + iframe) should be created.";

  content::RenderFrameHost* main_frame = contents->GetMainFrame();
  content::RenderFrameHost* child_frame =
    ChildFrameAt(contents->GetMainFrame(), 0);
  EXPECT_EQ(main_frame->GetLastCommittedURL(), url());

  std::string value;
  ASSERT_TRUE(ExecuteScriptAndExtractString(contents, kSetAndGetCookieScript, &value));
  EXPECT_STREQ(value.c_str(), COOKIE_STRING);

  ASSERT_TRUE(NavigateIframeToURL(contents, kIframeID, iframe_url()));
  ASSERT_EQ(child_frame->GetLastCommittedURL(), iframe_url());
  ASSERT_TRUE(ExecuteScriptAndExtractString(child_frame, kSetAndGetCookieScript, &value));
  EXPECT_STREQ(value.c_str(), COOKIE_STRING);
}
