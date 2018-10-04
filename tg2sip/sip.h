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

#ifndef TG2SIP_SIP_H
#define TG2SIP_SIP_H

#include <variant>
#include <pjsua2.hpp>
#include "logging.h"
#include "settings.h"
#include "queue.h"
#include "utils.h"

namespace sip {

    namespace events {
        struct IncomingCall {
            pjsua_call_id id;
            std::string extension;
        };

        struct CallStateUpdate {
            pjsua_call_id id;
            pjsip_inv_state state;
        };

        struct CallMediaStateUpdate {
            pjsua_call_id id;
            bool has_media;
        };

        typedef std::variant<IncomingCall, CallStateUpdate, CallMediaStateUpdate> Event;
    }

    class LogWriter : public pj::LogWriter {
    public:
        explicit LogWriter(std::shared_ptr<spdlog::logger> logger) : logger(std::move(logger)) {};

        ~LogWriter() override = default;

        void write(const pj::LogEntry &entry) override;

    private:
        std::shared_ptr<spdlog::logger> logger;
    };

    struct AccountConfig : public pj::AccountConfig {
        explicit AccountConfig(Settings &settings);
    };

    class Call : public pj::Call {
    public:
        Call(pj::Account &acc, std::shared_ptr<spdlog::logger> logger, int call_id);

        ~Call() override;

        void addHandler(std::function<void(pj::OnCallStateParam &)> &&handler);

        void addHandler(std::function<void(pj::OnCallMediaStateParam &)> &&handler);

        const std::string localUser();

        pj::AudioMedia *audio_media();


    private:
        friend class Client;

        void onCallState(pj::OnCallStateParam &prm) override;

        void onCallMediaState(pj::OnCallMediaStateParam &prm) override;

        std::function<void(pj::OnCallStateParam &)> onCallStateHandler;
        std::function<void(pj::OnCallMediaStateParam &)> onCallMediaStateHandler;
        std::string local_user_;
        std::shared_ptr<spdlog::logger> logger;
    };

    class Account : public pj::Account {
    public:
        explicit Account(std::shared_ptr<spdlog::logger> logger) : logger(std::move(logger)) {};

        void addHandler(std::function<void(pj::OnIncomingCallParam &)> &&handler);

    private:
        void onIncomingCall(pj::OnIncomingCallParam &prm) override;

        std::function<void(pj::OnIncomingCallParam &)> onIncomingCallHandler;
        std::shared_ptr<spdlog::logger> logger;
    };

    class Client {
    public:
        Client(std::unique_ptr<Account> account_, std::unique_ptr<AccountConfig> account_cfg_,
               OptionalQueue<events::Event> &events, Settings &settings,
               std::shared_ptr<spdlog::logger> logger_, LogWriter *sip_log_writer);

        Client(const Client &) = delete;

        Client &operator=(const Client &) = delete;

        virtual ~Client();

        void start();

        pjsua_call_id Dial(const std::string &uri, const pj::CallOpParam &prm);

        void Answer(pjsua_call_id call_id, const pj::CallOpParam &prm);

        void Hangup(pjsua_call_id call_id, const pj::CallOpParam &prm);

        void DialDtmf(pjsua_call_id call_id, const string &dtmf_digits);

        void BridgeAudio(pjsua_call_id call_id, pj::AudioMedia *input, pj::AudioMedia *output);

    private:
        std::shared_ptr<spdlog::logger> logger;

        std::unique_ptr<Account> account;
        std::unique_ptr<AccountConfig> account_cfg;

        OptionalQueue<events::Event> &events;
        std::map<int, std::shared_ptr<Call>> calls;

        static void init_pj_endpoint(Settings &settings, LogWriter *sip_log_writer);

        static std::string user_from_uri(const std::string &uri);

        void set_default_handlers(const std::shared_ptr<Call> &call);
    };
}

#endif //TG2SIP_SIP_H
