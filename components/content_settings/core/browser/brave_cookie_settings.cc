/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/components/content_settings/core/browser/brave_cookie_settings.h"

#include "brave/components/brave_shields/common/brave_shield_constants.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/gurl.h"

namespace {

bool IsValidSetting(ContentSetting setting) {
  return (setting == CONTENT_SETTING_ALLOW ||
          setting == CONTENT_SETTING_SESSION_ONLY ||
          setting == CONTENT_SETTING_BLOCK);
}

bool IsAllowed(ContentSetting setting) {
  DCHECK(IsValidSetting(setting));
  return (setting == CONTENT_SETTING_ALLOW ||
          setting == CONTENT_SETTING_SESSION_ONLY);
}

}  // namespace

namespace content_settings {

using namespace net::registry_controlled_domains;

BraveCookieSettings::BraveCookieSettings(
    HostContentSettingsMap* host_content_settings_map,
    PrefService* prefs,
    const char* extension_scheme)
    : CookieSettings(host_content_settings_map, prefs, extension_scheme)
{ }

BraveCookieSettings::~BraveCookieSettings() { }

void BraveCookieSettings::GetCookieSetting(const GURL& url,
    const GURL& first_party_url,
    content_settings::SettingSource* source,
    ContentSetting* cookie_setting) const {
  GetCookieSetting(url,
    first_party_url,
    first_party_url,
    source,
    cookie_setting);
}

void BraveCookieSettings::GetCookieSetting(const GURL& url,
    const GURL& first_party_url,
    const GURL& tab_url,
    content_settings::SettingSource* source,
    ContentSetting* cookie_setting) const {
  DCHECK(cookie_setting);
  CookieSettings::GetCookieSetting(url, first_party_url, source,
      cookie_setting);
  // Check the Brave shields setting, if it is off, just return without
  // blocking anything.
  ContentSetting brave_shields_setting =
      host_content_settings_map_->GetContentSetting(
          tab_url, GURL(),
          CONTENT_SETTINGS_TYPE_PLUGINS, brave_shields::kBraveShields);
  if (brave_shields_setting == CONTENT_SETTING_BLOCK) {
    return;
  }

  // Warning: Known bug where the first_party_url comes in as data, sometimes
  // In this case we don't know the real top level site for cookies and we end up
  // blocking because we don't know what the real origin is to get the setting for.
  ContentSetting brave_3p_setting =
      host_content_settings_map_->GetContentSetting(
          tab_url, GURL("https://notFirstParty/"),
          CONTENT_SETTINGS_TYPE_PLUGINS, brave_shields::kCookies);

  if (*cookie_setting == CONTENT_SETTING_ALLOW &&
          brave_3p_setting == CONTENT_SETTING_BLOCK &&
          // Empty first-party URL indicates a first-party request.
          !first_party_url.is_empty()) {

    if (SameDomainOrHost(url, tab_url, INCLUDE_PRIVATE_REGISTRIES)) {
      ContentSetting brave_setting = host_content_settings_map_->GetContentSetting(
        tab_url, GURL("https://firstParty/"),
        CONTENT_SETTINGS_TYPE_PLUGINS,
        brave_shields::kCookies);
      *cookie_setting = brave_setting;
    } else {
      *cookie_setting = CONTENT_SETTING_BLOCK;
    }
  }

}

bool BraveCookieSettings::IsCookieAccessAllowed(const GURL& url,
                                                const GURL& first_party_url,
                                                const GURL& tab_url) const {
  ContentSetting setting;
  GetCookieSetting(url, first_party_url, tab_url, nullptr, &setting);
  return IsAllowed(setting);
}

}  // namespace content_settings
