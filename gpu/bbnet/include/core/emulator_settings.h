// SPDX-License-Identifier: GPL-2.0-or-later
// Private settings for the native networking bridge. Renderer settings stay separate.
#pragma once
#include <atomic>
#include <cstdlib>
#include <string>

class BbNetSettingsImpl {
public:
    static BbNetSettingsImpl* GetInstance() { static BbNetSettingsImpl s; return &s; }
    static bool Flag(const char* name, bool fallback) {
        const char* v = std::getenv(name);
        return v ? v[0] == '1' : fallback;
    }
    static std::string Text(const char* name) {
        const char* v = std::getenv(name);
        return v ? v : "";
    }
    bool IsConnectedToNetwork() const { return Flag("BB_ONLINE", false); }
    bool IsShadNetEnabled() const { return IsConnectedToNetwork() && !shadnet_session_disabled.load(); }
    /// lbppc: LBPPC_NP_USER=<online id>: signed in locally as that player, no shadNet or PSN connection
    /// (the game's own servers are custom ones: run.py LBPPC_SERVER).
    std::string GetLocalNpUser() const {
        return Text("BB_SHADNET_NPID").empty() ? Text("LBPPC_NP_USER").substr(0, 16) : std::string{};
    }
    /// lbppc: the player's online id on a custom game server: the shadNet account (BB_SHADNET_NPID, real PSN-style
    /// sign-in with friends and P2P signaling) or the local one (LBPPC_NP_USER).
    std::string GetOnlineName() const {
        const std::string shadnet = Text("BB_SHADNET_NPID");
        return shadnet.empty() ? GetLocalNpUser() : shadnet.substr(0, 16);
    }
    void SetShadNetSessionDisabled(bool v) { shadnet_session_disabled.store(v); }
    std::string GetShadNetServer() const { return Text("BB_SHADNET_SERVER"); }
    /// lbppc: signed in locally, the PSN Web API is answered by the HTTP layer itself (http.cpp LocalWebApiHost).
    std::string GetShadNetWebApiServer() const {
        const std::string server = Text("BB_SHADNET_WEBAPI");
        return server.empty() && !Text("LBPPC_NP_USER").empty() ? "http://lbppc.local" : server;
    }
    bool IsUPnPEnabled() const { return Flag("BB_UPNP", false); }
    /// shadPS4's disable_https: HTTPS requests the game makes go out as plain HTTP.
    bool IsForcedHttpsDisabled() const { return Flag("BB_DISABLE_HTTPS", false); }
    /// The UDP port P2P traffic uses: 3658, the PS4's, unless BB_P2P_PORT (0: any free port).
    int GetP2PPort() const {
        const char* v = std::getenv("BB_P2P_PORT");
        return v && *v ? std::atoi(v) : 3658;
    }
private:
    std::atomic<bool> shadnet_session_disabled{false};
};
#define EmulatorSettings (*BbNetSettingsImpl::GetInstance())
