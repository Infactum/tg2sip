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

#ifndef TG2SIP_SETTINGS_H
#define TG2SIP_SETTINGS_H

#include <INIReader.h>

class Settings {
private:
    bool is_loaded_{false};

    int console_min_level_;
    int file_min_level_;
    int log_level_;
    int pjsip_log_level_;
    bool pjsip_log_sip_messages_;
    int tgvoip_log_level_;
    int tdlib_log_level_;

    unsigned int sip_port_;
    std::string id_uri_;
    std::string callback_uri_;
    std::string public_address_;
    std::string stun_server_;
    bool raw_pcm_;
    unsigned int sip_thread_count_;
    unsigned int sip_port_range_;

    int api_id_;
    std::string api_hash_;
    std::string db_folder_;
    std::string system_language_code_;
    std::string device_model_;
    std::string system_version_;
    std::string application_version_;

    bool udp_p2p_;
    bool udp_reflector_;

    bool aec_enabled_;
    bool ns_enabled_;
    bool agc_enabled_;

    bool proxy_enabled_;
    std::string proxy_address_;
    std::int32_t proxy_port_;
    std::string proxy_username_;
    std::string proxy_password_;

    bool voip_proxy_enabled_;
    std::string voip_proxy_address_;
    uint16_t voip_proxy_port_;
    std::string voip_proxy_username_;
    std::string voip_proxy_password_;

    unsigned int extra_wait_time_;
    unsigned int peer_flood_time_;

public:
    explicit Settings(INIReader &reader);

    Settings(const Settings &) = delete;

    Settings &operator=(const Settings &) = delete;

    bool is_loaded() const { return is_loaded_; };

    int console_min_level() const { return console_min_level_; };

    int file_min_level() const { return file_min_level_; };

    int log_level() const { return log_level_; };

    int pjsip_log_level() const { return pjsip_log_level_; };

    bool pjsip_log_sip_messages() const { return pjsip_log_sip_messages_; }

    int tgvoip_log_level() const { return tgvoip_log_level_; };

    int tdlib_log_level() const { return tdlib_log_level_; };

    unsigned int sip_port() const { return sip_port_; };

    string id_uri() const { return id_uri_; };

    string callback_uri() const { return callback_uri_; };

    string public_address() const { return public_address_; };

    string stun_server() const { return stun_server_; };

    bool raw_pcm() const { return raw_pcm_; };

    unsigned int sip_thread_count() const { return sip_thread_count_; };

    unsigned int sip_port_range() const { return sip_port_range_; };

    int api_id() const { return api_id_; };

    std::string api_hash() const { return api_hash_; };

    std::string db_folder() const { return db_folder_; };

    std::string sys_lang_code() const { return system_language_code_; };

    std::string device_model() const { return device_model_; };

    std::string system_version() const { return system_version_; };

    std::string app_version() const { return application_version_; };

    bool udp_p2p() const { return udp_p2p_; };

    bool udp_reflector() const { return udp_reflector_; };

    bool aec_enabled() const { return aec_enabled_; };

    bool ns_enabled() const { return ns_enabled_; };

    bool agc_enabled() const { return agc_enabled_; };

    bool proxy_enabled() const { return proxy_enabled_; };

    std::string proxy_address() const { return proxy_address_; };

    int32_t proxy_port() const { return proxy_port_; };

    std::string proxy_username() const { return proxy_username_; };

    std::string proxy_password() const { return proxy_password_; };

    bool voip_proxy_enabled() const { return voip_proxy_enabled_; };

    std::string voip_proxy_address() const { return voip_proxy_address_; };

    uint16_t voip_proxy_port() const { return voip_proxy_port_; };

    std::string voip_proxy_username() const { return voip_proxy_username_; };

    std::string voip_proxy_password() const { return voip_proxy_password_; };

    unsigned int extra_wait_time() const { return extra_wait_time_; };

    unsigned int peer_flood_time() const { return peer_flood_time_; };
};

#endif //TG2SIP_SETTINGS_H
