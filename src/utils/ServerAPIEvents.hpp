// Trimmed from https://github.com/GlobedGD/argon/blob/main/src/external/ServerAPIEvents.hpp
// (original: https://github.com/AlphiiGD/server-api/blob/main/include/ServerAPIEvents.hpp)
// Only read-only queries are kept (enough to detect the current server).
// Used only when km7dev.server_api is installed; creates no dependency.

#pragma once

#include <string>

#include <Geode/loader/Mod.hpp>
#include <Geode/loader/Dispatch.hpp>

#ifdef MY_MOD_ID
#undef MY_MOD_ID
#endif

#define MY_MOD_ID "km7dev.server_api"

namespace ServerAPIEvents {

/// @brief Represents server information
struct Server {
    /// @brief Handle for server info
    int id;
    /// @brief URL of the server (duh)
    ::std::string url;
    /// @brief ServerAPI will use the server with the highest priority for GD's online features
    int priority;
};

/// @brief Gets copy of the server currently in use by ServerAPI
/// @return Server struct representing server currently in use by ServerAPI
inline Server getCurrentServer() GEODE_EVENT_EXPORT_NORES(&getCurrentServer, ());

/// @brief Gets the base (https) URL for built-in servers
/// @return Base (https) URL for built-in servers (boomlings.com on vanilla GD)
inline ::std::string getBaseUrl() GEODE_EVENT_EXPORT_NORES(&getBaseUrl, ());

} // namespace ServerAPIEvents

#undef MY_MOD_ID
