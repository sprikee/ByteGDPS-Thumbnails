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
#include <cstring>
#include <algorithm>

#include "ServerAPIEvents.hpp"

#if !defined(_WIN32) && !defined(_WIN64)
#include <link.h>
#include <dlfcn.h>
#else
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace ByteGDPS {

inline bool isOurs(std::string_view url) {
    return url.find("bytegdps.ru") != std::string_view::npos
        || url.find("64.188.64.123") != std::string_view::npos
        || url.find("xn--80a0amh.fun") != std::string_view::npos;
}

// ---------- memory scanning fallback ----------
namespace detail {

inline bool containsNeedle(const char* haystack, size_t haystackSize, const char* needle) {
    size_t needleLen = std::strlen(needle);
    if (needleLen == 0 || haystackSize < needleLen) return false;
    const char* end = haystack + haystackSize - needleLen + 1;
    for (const char* p = haystack; p < end; ++p) {
        if (std::memcmp(p, needle, needleLen) == 0) return true;
    }
    return false;
}

inline bool scanGameLibFor(const char* needle) {
#if defined(_WIN32) || defined(GEODE_IS_WINDOWS)
    (void)needle;
    return false;
#else
    struct Payload {
        const char* needle;
        bool found = false;
    } payload{needle, false};
    dl_iterate_phdr([](struct dl_phdr_info* info, size_t, void* data) -> int {
        auto* p = reinterpret_cast<Payload*>(data);
        if (p->found) return 1;
        if (!info->dlpi_name) return 0;
        std::string name(info->dlpi_name);
        // game lib names: Android libcocos2dcpp.so, Windows/Mac GeometryDash, iOS similar
        bool isGame = false;
        if (name.find("libcocos2dcpp.so") != std::string::npos) isGame = true;
        else if (name.find("libcocos2d") != std::string::npos) isGame = true;
        else if (name.find("GeometryDash") != std::string::npos) isGame = true;
        else if (name.empty()) isGame = false; // skip main on Android (empty is not game lib)
        // On Android, the main app's lib is libcocos2dcpp.so, so above covers it
        // On Linux/Mac the main may be the executable itself (empty name handled differently)
        // To be safe, also scan the executable if name empty and it has LOAD segments with our needle?
        // We'll skip empty to avoid scanning every system lib
        if (!isGame) return 0;
        for (int i = 0; i < info->dlpi_phnum; ++i) {
            const auto& ph = info->dlpi_phdr[i];
            if (ph.p_type != PT_LOAD) continue;
            if (!(ph.p_flags & PF_R)) continue;
            const char* start = reinterpret_cast<const char*>(info->dlpi_addr + ph.p_vaddr);
            size_t sz = ph.p_memsz;
            if (!start || sz == 0 || sz > 100 * 1024 * 1024) continue;
            if (containsNeedle(start, sz, p->needle)) {
                p->found = true;
                return 1;
            }
        }
        return 0;
    }, &payload);
    return payload.found;
#endif
}

inline bool isByteGDPSInMemory() {
    return scanGameLibFor("bytegdps.ru");
}
inline bool isBoomlingsInMemory() {
    return scanGameLibFor("boomlings.com");
}

} // namespace detail

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
    off = 0;
#endif
    if (off) {
        const char* p = reinterpret_cast<const char*>(::geode::base::get() + off);
        if (p) {
            size_t len = 0;
            // safe scan up to 256, but ensure we don't segfault: we assume offset points into readable .rodata
            // If scanning hits unreadable, we fallback to memory scan
            bool readable = true;
#if defined(_WIN32) || defined(GEODE_IS_WINDOWS)
            // On Windows, check via IsBadReadPtr (deprecated but okay for gate)
            if (IsBadReadPtr(p, 1)) readable = false;
#endif
            if (readable) {
                while (len < 256 && p[len]) ++len;
                std::string ret(p, len);
                if (ret.size() > 34) ret = ret.substr(0, 34);
                while (!ret.empty() && ret.back() == '/') ret.pop_back();
                if (ret.rfind("http", 0) == 0) return ret;
            }
        }
    }
    // fallback: memory scan
    bool hasByte = detail::isByteGDPSInMemory();
    bool hasBoom = detail::isBoomlingsInMemory();
    if (hasByte && !hasBoom) return "https://bytegdps.ru";
    if (hasBoom && !hasByte) return "https://www.boomlings.com";
    if (hasByte && hasBoom) {
        // both present (should not happen in clean builds, but switcher may have duplicated)
        // prefer bytegdps if our domain is more recent? Check which appears more times? For now prefer byte
        return "https://bytegdps.ru";
    }
    return "";
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
