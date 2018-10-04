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

#ifndef TG2SIP_GATEWAY_H
#define TG2SIP_GATEWAY_H

#include <stdexcept>
#include <regex>
#include <libtgvoip/VoIPController.h>
#include <boost/sml.hpp>
#include <csignal>
#include "sip.h"
#include "tg.h"
#include "utils.h"
#include "queue.h"

namespace sml = boost::sml;

class Context;

struct Cache;

namespace state_machine::events {

    struct InternalError {
        std::string ctx_id;
        pj::CallOpParam prm;
    };

    typedef std::variant<InternalError> Event;
}

namespace state_machine::guards {
    struct IsIncoming {
        bool operator()(const td::td_api::object_ptr<td::td_api::updateCall> &event) const;
    };

    class IsInState {
    public:
        explicit IsInState(int32_t id) : id_(id) {}

        bool operator()(const td::td_api::object_ptr<td::td_api::updateCall> &event) const;

    private:
        const int32_t id_;
    };

    class IsSipInState {
    public:
        explicit IsSipInState(pjsip_inv_state state) : state_(state) {}

        bool operator()(const sip::events::CallStateUpdate &event) const;

    private:
        const pjsip_inv_state state_;
    };

    struct CallbackUriIsSet {
        bool operator()(const Settings &settings) const;
    };

    struct IsMediaReady {
        bool operator()(const sip::events::CallMediaStateUpdate &event) const;
    };

    struct IsTextContent {
        bool operator()(const td::td_api::object_ptr<td::td_api::updateNewMessage> &event) const;
    };

    struct IsDtmfString {
        bool operator()(const td::td_api::object_ptr<td::td_api::updateNewMessage> &event) const;
    };
}

namespace state_machine::actions {

    struct StoreTgId {
        void operator()(Context &ctx, const td::td_api::object_ptr<td::td_api::updateCall> &event,
                        std::shared_ptr<spdlog::logger> logger) const;
    };

    struct StoreTgUserId {
        void operator()(Context &ctx, const td::td_api::object_ptr<td::td_api::updateCall> &event,
                        std::shared_ptr<spdlog::logger> logger) const;
    };

    struct CleanTgId {
        void operator()(Context &ctx) const;
    };

    struct CleanSipId {
        void operator()(Context &ctx) const;
    };

    struct StoreSipId {
        void operator()(Context &ctx, const sip::events::IncomingCall &event,
                        std::shared_ptr<spdlog::logger> logger) const;
    };

    struct DialSip {
        void operator()(Context &ctx, tg::Client &tg_client, sip::Client &sip_client,
                        const td::td_api::object_ptr<td::td_api::updateCall> &event,
                        OptionalQueue<state_machine::events::Event> &internal_events,
                        const Settings &settings, std::shared_ptr<spdlog::logger> logger) const;
    };

    struct AnswerTg {
        void operator()(Context &ctx, tg::Client &tg_client, const Settings &settings,
                        OptionalQueue<state_machine::events::Event> &internal_events,
                        std::shared_ptr<spdlog::logger> logger) const;
    };

    struct AcceptIncomingSip {
        void operator()(Context &ctx, sip::Client &sip_client, const Settings &settings,
                        const sip::events::IncomingCall &event,
                        OptionalQueue<state_machine::events::Event> &internal_events,
                        std::shared_ptr<spdlog::logger> logger) const;
    };

    struct AnswerSip {
        void operator()(Context &ctx, sip::Client &sip_client, const Settings &settings,
                        OptionalQueue<state_machine::events::Event> &internal_events,
                        std::shared_ptr<spdlog::logger> logger) const;
    };

    struct BridgeAudio {
        void operator()(Context &ctx, sip::Client &sip_client,
                        OptionalQueue<state_machine::events::Event> &internal_events,
                        std::shared_ptr<spdlog::logger> logger) const;
    };

    struct CreateTgVoip {
        void operator()(Context &ctx, const Settings &settings,
                        const td::td_api::object_ptr<td::td_api::updateCall> &event,
                        std::shared_ptr<spdlog::logger> logger) const;
    };

    struct CleanUp {
        void operator()(Context &ctx, sip::Client &sip_client, tg::Client &tg_client,
                        std::shared_ptr<spdlog::logger> logger) const;
    };

    struct SetHangupPrm {
        void operator()(Context &ctx, const state_machine::events::InternalError &event) const;
    };

    class DialTg {
    private:
        Context *ctx_;
        tg::Client *tg_client_;
        Settings const *settings_;
        Cache *cache_;
        std::chrono::steady_clock::time_point *block_until_;
        OptionalQueue<state_machine::events::Event> *internal_events_;
        std::shared_ptr<spdlog::logger> logger_;

        void parse_error(td::td_api::object_ptr<td::td_api::error> error);

        void dial_by_id(int32_t id);

        void dial_by_phone();

        void dial_by_username();

    public:
        void operator()(Context &ctx, tg::Client &tg_client, const Settings &settings, Cache &cache,
                        std::chrono::steady_clock::time_point &block_until,
                        OptionalQueue<state_machine::events::Event> &internal_events,
                        std::shared_ptr<spdlog::logger> logger);
    };

    struct DialDtmf {
        void operator()(Context &ctx, sip::Client &sip_client,
                        const td::td_api::object_ptr<td::td_api::updateNewMessage> &event,
                        std::shared_ptr<spdlog::logger> logger) const;
    };
}

namespace state_machine {
    class Logger {
    public:
        Logger(std::string context_id, shared_ptr<spdlog::logger> logger);

        virtual ~Logger();

        template<class SM>
        void log_process_event(const td::td_api::object_ptr<td::td_api::updateCall> &event);

        template<class SM>
        void log_process_event(const sip::events::Event &event);

        template<class SM, class TEvent>
        void log_process_event(const TEvent &);

        template<class SM, class TGuard, class TEvent>
        void log_guard(const TGuard &, const TEvent &event, bool result);

        template<class SM, class TAction, class TEvent>
        void log_action(const TAction &, const TEvent &event);

        template<class SmLogger, class TSrcState, class TDstState>
        void log_state_change(const TSrcState &src, const TDstState &dst);

    private:
        const std::string context_id_;
        std::shared_ptr<spdlog::logger> logger_;
    };

    struct StateMachine;

    typedef sml::sm<StateMachine, sml::logger<Logger>, sml::thread_safe<std::recursive_mutex>> sm_t;
}

struct Cache {
    std::map<std::string, int32_t> username_cache;
    std::map<std::string, int32_t> phone_cache;
};

class Context {
public:
    Context();

    const std::string id() const;

    pjsua_call_id sip_call_id{PJSUA_INVALID_ID};
    int32_t tg_call_id{0};
    std::shared_ptr<tgvoip::VoIPController> controller{nullptr};

    std::string ext_phone;
    std::string ext_username;
    int32_t user_id{0};

    pj::CallOpParam hangup_prm;

private:
    const std::string id_;

    std::string next_ctx_id();
};

struct Bridge {
    std::unique_ptr<state_machine::sm_t> sm;
    std::unique_ptr<Context> ctx;
    std::unique_ptr<state_machine::Logger> logger;
};

class Gateway {
public:
    Gateway(sip::Client &sip_client_, tg::Client &tg_client_,
            OptionalQueue<sip::events::Event> &sip_events_,
            OptionalQueue<tg::Client::Object> &tg_events_,
            std::shared_ptr<spdlog::logger> logger_,
            Settings &settings);

    Gateway(const Gateway &) = delete;

    Gateway &operator=(const Gateway &) = delete;

    void start(volatile sig_atomic_t e_flag);

private:
    std::shared_ptr<spdlog::logger> logger_;

    sip::Client &sip_client_;
    tg::Client &tg_client_;
    const Settings &settings_;

    OptionalQueue<sip::events::Event> &sip_events_;
    OptionalQueue<tg::Client::Object> &tg_events_;
    OptionalQueue<state_machine::events::Event> internal_events_;

    std::chrono::steady_clock::time_point block_until{std::chrono::steady_clock::now()};
    Cache cache_;
    // Would be better to use smart pointer here,
    // but it is not allowed by sm_t forward declaration
    std::vector<Bridge *> bridges;

    std::vector<Bridge *>::iterator search_call(const std::function<bool(const Bridge *)> &predicate);

    void process_event(td::td_api::object_ptr<td::td_api::updateCall> update_call);

    void process_event(td::td_api::object_ptr<td::td_api::updateNewMessage> update_message);

    void process_event(state_machine::events::InternalError &event);

    template<typename TSipEvent>
    void process_event(const TSipEvent &event);

    void load_cache();
};

#endif //TG2SIP_GATEWAY_H
