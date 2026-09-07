#pragma once
// ByteGDPS gate: our mods only act while the game is connected to ByteGDPS.
// On vanilla (or any other server) every feature stays inert, so our data
// (thumbnails, demonlist badges) never leaks onto foreign levels.
// Detection: ServerAPI current server when present, otherwise the server URL
// string in the game binary (this is what switchers patch). Fail-open when
// the URL cannot be read at all, fail-closed on a positively foreign server.
#include <Geode/loader/Hook.hpp>
#include <Geode/loader/Loader.hpp>
#include <Geode/loader/Mod.hpp>

#include <string>
#include <string_view>

#include "ServerAPIEvents.hpp"

namespace ByteGDPS {

inline bool isOurs(std::string_view url) {
    return url.find("bytegdps.ru") != std::string_view::npos
        || url.find("64.188.64.123") != std::string_view::npos
        || url.find("xn--80a0amh.fun") != std::string_view::npos;
}

// URL the game binary is currently pointed at (GD 2.2081 string offsets).
inline std::string gameServerUrl() {
    uintptr_t off = 0;
#if defined(GEODE_IS_WINDOWS)
    off = 0x558b70;
#elif defined(GEODE_IS_ANDROID64)
    off = 0xeccf90;
#elif defined(GEODE_IS_ANDROID32)
    off = 0x96c0db;
#elif defined(GEODE_IS_ARM_MAC)
    off = 0x77d709;
#elif defined(GEODE_IS_INTEL_MAC)
    off = 0x868df0;
#elif defined(GEODE_IS_IOS)
    off = 0x6b8cc2;
#else
    return "";
#endif
    if (!off) return "";
    const char* p = reinterpret_cast<const char*>(::geode::base::get() + off);
    if (!p) return "";
    size_t len = 0;
    while (len < 256 && p[len]) ++len;
    std::string ret(p, len);
    if (ret.size() > 34) ret = ret.substr(0, 34);
    while (!ret.empty() && ret.back() == '/') ret.pop_back();
    if (ret.rfind("http", 0) != 0) return "";
    return ret;
}

inline std::string currentServerUrl() {
    if (::geode::Loader::get()->isModLoaded("km7dev.server_api")) {
        auto s = ServerAPIEvents::getCurrentServer();
        if (!s.url.empty() && s.url != "NONE_REGISTERED") {
            std::string u = s.url;
            while (!u.empty() && u.back() == '/') u.pop_back();
            return u;
        }
    }
    return gameServerUrl();
}

// True when the mod should act. Reads the "bytegdps-only" setting live,
// so toggling it applies immediately without restart.
inline bool isActive() {
    try {
        auto mod = ::geode::Mod::get();
        if (mod && !mod->getSettingValue<bool>("bytegdps-only")) return true;
    } catch (...) {}
    auto url = currentServerUrl();
    if (url.empty()) return true;
    return isOurs(url);
}

} // namespace ByteGDPS
