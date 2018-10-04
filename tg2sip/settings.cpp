/*
 * Copyright (C) 2017-2018 infactum (infactum@gmail.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; If not, see <https://www.gnu.org/licenses/>.
 */

#include <iostream>
#include <thread>
#include "settings.h"

Settings::Settings(INIReader &reader) {

    if (reader.ParseError() < 0) {
        std::cerr << "Can't load settings file!\n";
        return;
    }

    // logging
    console_min_level_ = std::clamp(static_cast<int>(reader.GetInteger("logging", "console_min_level", 0)), 0, 6);
    file_min_level_ = std::clamp(static_cast<int>(reader.GetInteger("logging", "file_min_level", 0)), 0, 6);
    log_level_ = std::clamp(static_cast<int>(reader.GetInteger("logging", "core", 2)), 0, 6);
    tdlib_log_level_ = std::clamp(static_cast<int>(reader.GetInteger("logging", "tdlib", 3)), 0, 6);
    tgvoip_log_level_ = std::clamp(static_cast<int>(reader.GetInteger("logging", "tgvoip", 5)), 0, 6);
    pjsip_log_level_ = std::clamp(static_cast<int>(reader.GetInteger("logging", "pjsip", 2)), 0, 6);
    pjsip_log_sip_messages_ = reader.GetBoolean("logging", "sip_messages", true);

    // sip
    sip_port_ = static_cast<unsigned int>(reader.GetInteger("sip", "port", 0));
    id_uri_ = reader.Get("sip", "id_uri", "sip:localhost");
    callback_uri_ = reader.Get("sip", "callback_uri", "");
    public_address_ = reader.Get("sip", "public_address", "");
    stun_server_ = reader.Get("sip", "stun_server", "");
    raw_pcm_ = reader.GetBoolean("sip", "raw_pcm", true);
    sip_thread_count_ = static_cast<unsigned int>(reader.GetInteger("sip", "thread_count", 1));
    sip_port_range_ = static_cast<unsigned int>(reader.GetInteger("sip", "port_range", 0));

    //telegram
    api_id_ = static_cast<int>(reader.GetInteger("telegram", "api_id", 0));
    api_hash_ = reader.Get("telegram", "api_hash", "");
    db_folder_ = reader.Get("telegram", "database_folder", "");
    system_language_code_ = reader.Get("telegram", "database_folder", "en-US");
    device_model_ = reader.Get("telegram", "device_model", "PC");
    system_version_ = reader.Get("telegram", "system_version", "Linux");
    application_version_ = reader.Get("telegram", "application_version", "1.0");

    udp_p2p_ = reader.GetBoolean("telegram", "udp_p2p", false);
    udp_reflector_ = reader.GetBoolean("telegram", "udp_reflector", true);

    aec_enabled_ = reader.GetBoolean("telegram", "enable_aec", false);
    ns_enabled_ = reader.GetBoolean("telegram", "enable_ns", false);
    agc_enabled_ = reader.GetBoolean("telegram", "enable_agc", false);

    proxy_enabled_ = reader.GetBoolean("telegram", "use_proxy", false);
    proxy_address_ = reader.Get("telegram", "proxy_address", "");
    proxy_port_ = static_cast<int32_t>(reader.GetInteger("telegram", "proxy_port", 0));
    proxy_username_ = reader.Get("telegram", "proxy_username", "");
    proxy_password_ = reader.Get("telegram", "proxy_password", "");

    voip_proxy_enabled_ = reader.GetBoolean("telegram", "use_voip_proxy", false);
    voip_proxy_address_ = reader.Get("telegram", "voip_proxy_address", "");
    voip_proxy_port_ = static_cast<uint16_t>(reader.GetInteger("telegram", "voip_proxy_port", 0));
    voip_proxy_username_ = reader.Get("telegram", "voip_proxy_username", "");
    voip_proxy_password_ = reader.Get("telegram", "voip_proxy_password", "");

    extra_wait_time_ = static_cast<unsigned int>(reader.GetInteger("other", "extra_wait_time", 30));
    peer_flood_time_ = static_cast<unsigned int>(reader.GetInteger("other", "peer_flood_time", 86400));

    if (api_id_ == 0 || api_hash_.empty()) {
        std::cerr << "TDLib api settings must be set!\n";
        return;
    }

    is_loaded_ = true;
}
