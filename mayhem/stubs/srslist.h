#pragma once
// Offline stand-in for the generated rule-set index that upstream's CI downloads
// (routeprofiles/rule-set/srslist.h): a name-sorted table of name -> .srs URL.
#include <array>
#include <string_view>
#include <utility>

inline constexpr std::array<std::pair<std::string_view, std::string_view>, 2> ruleSetList{{
    {"geoip-cn", "https://example.invalid/rule-set/geoip-cn.srs"},
    {"geosite-cn", "https://example.invalid/rule-set/geosite-cn.srs"},
}};
