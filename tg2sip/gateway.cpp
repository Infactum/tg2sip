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

#include <regex>
#include "gateway.h"

namespace sml = boost::sml;
namespace td_api = td::td_api;

#define CALL_PROTO_MIN_LAYER 65

volatile sig_atomic_t e_flag = 0;

namespace state_machine::guards {
    bool IsIncoming::operator()(const td::td_api::object_ptr<td::td_api::updateCall> &event) const {
        return !event->call_->is_outgoing_;
    }

    bool IsInState::operator()(const td::td_api::object_ptr<td::td_api::updateCall> &event) const {
        return event->call_->state_->get_id() == id_;
    }

    bool CallbackUriIsSet::operator()(const Settings &settings) const {
        return !settings.callback_uri().empty();
    }

    bool IsMediaReady::operator()(const sip::events::CallMediaStateUpdate &event) const {
        return event.has_media;
    }

    bool IsSipInState::operator()(const sip::events::CallStateUpdate &event) const {
        return event.state == state_;
    }

    bool IsTextContent::operator()(const td::td_api::object_ptr<td::td_api::updateNewMessage> &event) const {
        return event->message_->content_->get_id() == td::td_api::messageText::ID;
    }

    bool IsDtmfString::operator()(const td::td_api::object_ptr<td::td_api::updateNewMessage> &event) const {

        // String must contain only characters as described on RFC 2833 section 3.10.
        // if PJMEDIA_HAS_DTMF_FLASH is enabled, character 'R' is used to represent
        // the event type 16 (flash) as stated in RFC 4730.
        // PJSUA maximum number of characters are 32.

        auto content = td_api::move_object_as<td_api::messageText>(event->message_->content_);
        const std::regex dtmf_regex("^[0-9A-D*#]{1,32}$");
        auto result = regex_match(content->text_->text_, dtmf_regex);

        // Don't forget to put data back!
        event->message_->content_ = td_api::move_object_as<td_api::MessageContent>(content);

        return result;
    }
}

namespace state_machine::actions {
    void StoreSipId::operator()(Context &ctx, const sip::events::IncomingCall &event,
                                std::shared_ptr<spdlog::logger> logger) const {
        ctx.sip_call_id = event.id;
        DEBUG(logger, "[{}] associated with SIP#{}", ctx.id(), ctx.sip_call_id);
    }

    void StoreTgId::operator()(Context &ctx, const td::td_api::object_ptr<td::td_api::updateCall> &event,
                               std::shared_ptr<spdlog::logger> logger) const {
        ctx.tg_call_id = event->call_->id_;
        DEBUG(logger, "[{}] associated with TG#{}", ctx.id(), ctx.tg_call_id);
    }

    void StoreTgUserId::operator()(Context &ctx, const td::td_api::object_ptr<td::td_api::updateCall> &event,
                                   std::shared_ptr<spdlog::logger> logger) const {
        ctx.user_id = event->call_->user_id_;
        DEBUG(logger, "[{}] stored user id {}", ctx.id(), ctx.user_id);
    }

    void CleanTgId::operator()(Context &ctx) const {
        ctx.tg_call_id = 0;
    }

    void CleanSipId::operator()(Context &ctx) const {
        ctx.sip_call_id = PJSUA_INVALID_ID;
    }


    void CleanUp::operator()(Context &ctx, sip::Client &sip_client, tg::Client &tg_client,
                             std::shared_ptr<spdlog::logger> logger) const {
        TRACE(logger, "[{}] cleanup start", ctx.id());

        if (ctx.controller) {
            ctx.controller->Stop();
        }

        if (ctx.tg_call_id != 0) {
            DEBUG(logger, "[{}] hangup TG #{}", ctx.id(), ctx.tg_call_id);

            auto result = tg_client.send_query_async(td_api::make_object<td_api::discardCall>(
                    ctx.tg_call_id, /* call_id_ */
                    false, /* is_disconnected_ */
                    0, /* duration_ */
                    ctx.tg_call_id /*connection_id */
            )).get();

            if (result->get_id() == td_api::error::ID) {
                logger->error("[{}] TG call discard failure:\n{}", ctx.id(), to_string(result));
            }

            ctx.tg_call_id = 0;
        }

        if (ctx.sip_call_id != PJSUA_INVALID_ID) {
            try {
                sip_client.Hangup(ctx.sip_call_id, ctx.hangup_prm);
            } catch (const pj::Error &error) {
                logger->error(error.reason);
            }
            ctx.sip_call_id = PJSUA_INVALID_ID;
        }

        TRACE(logger, "[{}] cleanup end");
    }

    void DialSip::operator()(Context &ctx, tg::Client &tg_client, sip::Client &sip_client,
                             const td_api::object_ptr<td_api::updateCall> &event,
                             OptionalQueue<state_machine::events::Event> &internal_events,
                             const Settings &settings, std::shared_ptr<spdlog::logger> logger) const {

        auto tg_user_id = event->call_->user_id_;
        auto response = tg_client.send_query_async(td_api::make_object<td_api::getUser>(tg_user_id)).get();

        if (response->get_id() == td_api::error::ID) {
            logger->error("[{}] get user info of id {} failed\n{}", ctx.id(), tg_user_id, to_string(response));
            auto err = td_api::move_object_as<td_api::error>(response);
            throw std::runtime_error{err->message_};
        }

        auto user = td::move_tl_object_as<td_api::user>(response);
        auto prm = pj::CallOpParam(true);

        auto &headers = prm.txOption.headers;
        auto header = pj::SipHeader();

        {
            // debug purpose header
            header.hName = "X-GW-Context";
            header.hValue = ctx.id();
            headers.push_back(header);
        }

        {
            header.hName = "X-TG-ID";
            header.hValue = std::to_string(tg_user_id);
            headers.push_back(header);
        }

        if (!user->first_name_.empty()) {
            header.hName = "X-TG-FirstName";
            header.hValue = user->first_name_;
            headers.push_back(header);
        }

        if (!user->last_name_.empty()) {
            header.hName = "X-TG-LastName";
            header.hValue = user->last_name_;
            headers.push_back(header);
        }

        if (!user->username_.empty()) {
            header.hName = "X-TG-Username";
            header.hValue = user->username_;
            headers.push_back(header);
        }

        if (!user->phone_number_.empty()) {
            header.hName = "X-TG-Phone";
            header.hValue = user->phone_number_;
            headers.push_back(header);
        }

        try {
            ctx.sip_call_id = sip_client.Dial(settings.callback_uri(), prm);
        } catch (const pj::Error &error) {
            pj::CallOpParam hangup_prm;
            hangup_prm.statusCode = PJSIP_SC_INTERNAL_SERVER_ERROR;
            hangup_prm.reason = error.reason;

            internal_events.emplace(state_machine::events::InternalError{ctx.id(), hangup_prm});

            return;
        }

        DEBUG(logger, "[{}] associated with SIP#{}", ctx.id(), ctx.sip_call_id);
    }

    void AnswerTg::operator()(Context &ctx, tg::Client &tg_client, const Settings &settings,
                              OptionalQueue<state_machine::events::Event> &internal_events,
                              std::shared_ptr<spdlog::logger> logger) const {

        auto response = tg_client.send_query_async(td_api::make_object<td_api::acceptCall>(
                ctx.tg_call_id,
                td_api::make_object<td_api::callProtocol>(settings.udp_p2p(),
                                                          settings.udp_reflector(),
                                                          CALL_PROTO_MIN_LAYER,
                                                          tgvoip::VoIPController::GetConnectionMaxLayer())
        )).get();

        if (response->get_id() == td_api::error::ID) {
            logger->error("[{}] TG #{} accept failure\n{}", ctx.id(), ctx.tg_call_id, to_string(response));

            auto error = td::move_tl_object_as<td_api::error>(response);

            pj::CallOpParam hangup_prm;
            hangup_prm.statusCode = PJSIP_SC_INTERNAL_SERVER_ERROR;
            hangup_prm.reason = std::to_string(error->code_) + "; " + error->message_;

            internal_events.emplace(state_machine::events::InternalError{ctx.id(), hangup_prm});
        }

    }

    void AcceptIncomingSip::operator()(Context &ctx, sip::Client &sip_client, const Settings &settings,
                                       const sip::events::IncomingCall &event,
                                       OptionalQueue<state_machine::events::Event> &internal_events,
                                       std::shared_ptr<spdlog::logger> logger) const {
        auto ext = event.extension;

        bool ext_valid{true};

        if (ext.length() > 3 && ext.substr(0, 3) == "tg#") {
            ctx.ext_username = ext.substr(3, std::string::npos);
        } else if (ext.length() > 1 && ext[0] == '+' && is_digits(ext.substr(1, std::string::npos))) {
            ctx.ext_phone = ext.substr(1, std::string::npos);
        } else if (is_digits(ext)) {
            try {
                ctx.user_id = std::stoi(ext);
            } catch (const std::invalid_argument &e) {
                ext_valid = false;
            } catch (const std::out_of_range &e) {
                ext_valid = false;
            }
        } else {
            ext_valid = false;
        }

        if (ext_valid) {
            auto a_prm = pj::CallOpParam(true);
            a_prm.statusCode = PJSIP_SC_RINGING;
            DEBUG(logger, "[{}] setting SIP #{} in ringing mode", ctx.id(), ctx.sip_call_id);
            try {
                sip_client.Answer(ctx.sip_call_id, a_prm);
            } catch (const pj::Error &error) {
                pj::CallOpParam hangup_prm;
                hangup_prm.statusCode = PJSIP_SC_INTERNAL_SERVER_ERROR;
                hangup_prm.reason = error.reason;
                internal_events.emplace(state_machine::events::InternalError{ctx.id(), hangup_prm});
            } catch (const std::runtime_error &error) {
                pj::CallOpParam hangup_prm;
                hangup_prm.statusCode = PJSIP_SC_INTERNAL_SERVER_ERROR;
                hangup_prm.reason = error.what();
                internal_events.emplace(state_machine::events::InternalError{ctx.id(), hangup_prm});
            }

        } else {
            pj::CallOpParam prm;
            prm.statusCode = PJSIP_SC_BAD_EXTENSION;
            logger->warn("[{}] called invalid extension {}", ctx.id(), ext);
            internal_events.emplace(state_machine::events::InternalError{ctx.id(), prm});
        }
    }

    void AnswerSip::operator()(Context &ctx, sip::Client &sip_client, const Settings &settings,
                               OptionalQueue<state_machine::events::Event> &internal_events,
                               std::shared_ptr<spdlog::logger> logger) const {

        auto prm = pj::CallOpParam(true);
        prm.statusCode = PJSIP_SC_OK;
        DEBUG(logger, "[{}] answering SIP #{}", ctx.id(), ctx.sip_call_id);

        try {
            sip_client.Answer(ctx.sip_call_id, prm);
        } catch (const pj::Error &error) {
            pj::CallOpParam hangup_prm;
            hangup_prm.statusCode = PJSIP_SC_INTERNAL_SERVER_ERROR;
            hangup_prm.reason = error.reason;
            internal_events.emplace(state_machine::events::InternalError{ctx.id(), hangup_prm});
        } catch (const std::runtime_error &error) {
            pj::CallOpParam hangup_prm;
            hangup_prm.statusCode = PJSIP_SC_INTERNAL_SERVER_ERROR;
            hangup_prm.reason = error.what();
            internal_events.emplace(state_machine::events::InternalError{ctx.id(), hangup_prm});
        }
    }

    void state_machine::actions::CreateTgVoip::operator()(Context &ctx, const Settings &settings,
                                                          const td::td_api::object_ptr<td::td_api::updateCall> &event,
                                                          std::shared_ptr<spdlog::logger> logger) const {

        DEBUG(logger, "[{}] creating voip for TG #{}", ctx.id(), ctx.tg_call_id);

        using namespace tgvoip;

        auto state = td_api::move_object_as<td_api::callStateReady>(event->call_->state_);
        auto voip_controller = std::make_shared<VoIPController>();

        static auto config = VoIPController::Config(
                3000,   /*init_timeout*/
                3000,   /*recv_timeout*/
                DATA_SAVING_NEVER, /*data_saving*/
                settings.aec_enabled(),   /*enableAEC*/
                settings.ns_enabled(),  /*enableNS*/
                settings.agc_enabled(),  /*enableAGC*/
                false   /*enableCallUpgrade*/
        );
        voip_controller->SetConfig(config);
        if (settings.voip_proxy_enabled()) {
            voip_controller->SetProxy(
                    PROXY_SOCKS5,
                    settings.voip_proxy_address(),
                    settings.voip_proxy_port(),
                    settings.voip_proxy_username(),
                    settings.voip_proxy_password()
            );
        }

        char encryption_key[256];
        memcpy(encryption_key, state->encryption_key_.c_str(), 256);
        voip_controller->SetEncryptionKey(encryption_key, event->call_->is_outgoing_);

        vector<Endpoint> endpoints;
        for (auto &connection : state->connections_) {
            unsigned char peer_tag[16];
            memcpy(peer_tag, connection->peer_tag_.c_str(), 16);
            auto ipv4 = IPv4Address(connection->ip_);
            auto ipv6 = IPv6Address(connection->ipv6_);
            endpoints.emplace_back(Endpoint(connection->id_,
                                            static_cast<uint16_t>(connection->port_),
                                            ipv4,
                                            ipv6,
                                            Endpoint::UDP_RELAY,
                                            peer_tag));
        }
        voip_controller->SetRemoteEndpoints(endpoints, settings.udp_p2p(), VoIPController::GetConnectionMaxLayer());
        voip_controller->Start();
        voip_controller->Connect();

        ctx.controller = std::move(voip_controller);
    }

    void state_machine::actions::BridgeAudio::operator()(Context &ctx, sip::Client &sip_client,
                                                         OptionalQueue<state_machine::events::Event> &internal_events,
                                                         std::shared_ptr<spdlog::logger> logger) const {
        DEBUG(logger, "[{}] bridging tgvoip audio with SIP#{}", ctx.id(), ctx.sip_call_id);

        try {
            sip_client.BridgeAudio(ctx.sip_call_id,
                                   ctx.controller->AudioMediaInput(),
                                   ctx.controller->AudioMediaOutput());
        } catch (const pj::Error &error) {
            pj::CallOpParam hangup_prm;
            hangup_prm.statusCode = PJSIP_SC_INTERNAL_SERVER_ERROR;
            hangup_prm.reason = error.reason;
            internal_events.emplace(state_machine::events::InternalError{ctx.id(), hangup_prm});
        } catch (const std::runtime_error &error) {
            pj::CallOpParam hangup_prm;
            hangup_prm.statusCode = PJSIP_SC_INTERNAL_SERVER_ERROR;
            hangup_prm.reason = error.what();
            internal_events.emplace(state_machine::events::InternalError{ctx.id(), hangup_prm});
        }
    }

    void SetHangupPrm::operator()(Context &ctx, const state_machine::events::InternalError &event) const {
        ctx.hangup_prm = event.prm;
    }

    void DialTg::operator()(Context &ctx, tg::Client &tg_client, const Settings &settings, Cache &cache,
                            std::chrono::steady_clock::time_point &block_until,
                            OptionalQueue<state_machine::events::Event> &internal_events,
                            std::shared_ptr<spdlog::logger> logger) {

        DEBUG(logger, "[{}] dialing tg", ctx.id());

        ctx_ = &ctx;
        tg_client_ = &tg_client;
        settings_ = &settings;
        cache_ = &cache;
        block_until_ = &block_until;
        internal_events_ = &internal_events;
        logger_ = logger;

        if (block_until > std::chrono::steady_clock::now()) {
            auto seconds_left = std::chrono::duration_cast<std::chrono::seconds>(
                    block_until - std::chrono::steady_clock::now()).count();

            DEBUG(logger, "[{}] dropping call cause of temp TG block for {} seconds", ctx.id(), seconds_left);

            pj::CallOpParam prm;
            prm.statusCode = PJSIP_SC_INTERNAL_SERVER_ERROR;
            prm.reason = "FLOOD_WAIT " + std::to_string(seconds_left);
            internal_events.emplace(state_machine::events::InternalError{ctx.id(), prm});
            return;
        }

        if (!ctx.ext_username.empty()) {
            dial_by_username();
        } else if (!ctx.ext_phone.empty()) {
            dial_by_phone();
        } else {
            dial_by_id(ctx.user_id);
        }
    }

    void DialTg::dial_by_id(int32_t id) {
        auto response = tg_client_->send_query_async(td_api::make_object<td_api::createCall>(
                id /* id */,
                td_api::make_object<td_api::callProtocol>(settings_->udp_p2p(), settings_->udp_reflector(),
                                                          CALL_PROTO_MIN_LAYER,
                                                          tgvoip::VoIPController::GetConnectionMaxLayer()))
        ).get();

        if (response->get_id() == td_api::error::ID) {
            logger_->error("[{}] failed to create telegram call\n{}", ctx_->id(), to_string(response));

            auto error = td::move_tl_object_as<td_api::error>(response);

            auto prm = pj::CallOpParam(true);
            prm.statusCode = PJSIP_SC_INTERNAL_SERVER_ERROR;
            prm.reason = std::to_string(error->code_) + "; " + error->message_;

            internal_events_->emplace(state_machine::events::InternalError{ctx_->id(), prm});

            parse_error(std::move(error));
            return;
        }

        auto call_id_ = td::move_tl_object_as<td_api::callId>(response);

        DEBUG(logger_, "[{}] associated with TG#{}", ctx_->id(), call_id_->id_);
        ctx_->tg_call_id = call_id_->id_;
    }

    void DialTg::dial_by_phone() {

        auto it = cache_->phone_cache.find(ctx_->ext_phone);
        if (it != cache_->phone_cache.end()) {
            DEBUG(logger_, "[{}] found id {} for {} in phone cache", ctx_->id(), it->second, ctx_->ext_phone);
            dial_by_id(it->second);
            return;
        }

        auto contact = td_api::make_object<td_api::contact>();
        contact->phone_number_ = ctx_->ext_phone;

        auto contacts = std::vector<td_api::object_ptr<td_api::contact>>();
        contacts.emplace_back(std::move(contact));

        auto response = tg_client_->send_query_async(
                td_api::make_object<td_api::importContacts>(std::move(contacts))).get();

        if (response->get_id() == td_api::error::ID) {
            logger_->error("[{}] contacts import failure\n{}", ctx_->id(), to_string(response));
            auto error = td::move_tl_object_as<td_api::error>(response);

            auto prm = pj::CallOpParam(true);
            prm.statusCode = PJSIP_SC_INTERNAL_SERVER_ERROR;
            prm.reason = std::to_string(error->code_) + "; " + error->message_;
            internal_events_->emplace(state_machine::events::InternalError{ctx_->id(), prm});

            parse_error(std::move(error));
            return;
        }

        auto imported_contacts = td::move_tl_object_as<td_api::importedContacts>(response);
        auto user_id_ = imported_contacts->user_ids_[0];

        if (user_id_ == 0) {
            logger_->error("[{}] {} is not telegram user yet", ctx_->id(), ctx_->ext_phone);

            auto prm = pj::CallOpParam(true);
            prm.statusCode = PJSIP_SC_NOT_FOUND;
            prm.reason = "not registered in telegram";
            internal_events_->emplace(state_machine::events::InternalError{ctx_->id(), prm});

            return;
        }

        DEBUG(logger_, "[{}] adding id {} for {} to phone cache", ctx_->id(), user_id_, ctx_->ext_phone);
        cache_->phone_cache.emplace(ctx_->ext_phone, user_id_);
        dial_by_id(user_id_);
    }

    void DialTg::dial_by_username() {
        auto it = cache_->username_cache.find(ctx_->ext_username);
        if (it != cache_->username_cache.end()) {
            DEBUG(logger_, "[{}] found id {} for {} in username cache", ctx_->id(), it->second, ctx_->ext_username);
            dial_by_id(it->second);
            return;
        }

        auto response = tg_client_->send_query_async(
                td_api::make_object<td_api::searchPublicChat>(ctx_->ext_username)).get();

        if (response->get_id() == td_api::error::ID) {
            logger_->error("[{}] chat request failure\n{}", ctx_->id(), to_string(response));

            auto error = td::move_tl_object_as<td_api::error>(response);

            auto prm = pj::CallOpParam(true);
            prm.statusCode = PJSIP_SC_INTERNAL_SERVER_ERROR;
            prm.reason = std::to_string(error->code_) + "; " + error->message_;
            internal_events_->emplace(state_machine::events::InternalError{ctx_->id(), prm});

            parse_error(std::move(error));
            return;
        }

        auto chat = td::move_tl_object_as<td_api::chat>(response);

        if (chat->type_->get_id() != td_api::chatTypePrivate::ID) {

            auto prm = pj::CallOpParam(true);
            prm.statusCode = PJSIP_SC_INTERNAL_SERVER_ERROR;
            prm.reason = "not a user";
            internal_events_->emplace(state_machine::events::InternalError{ctx_->id(), prm});

            return;
        }

        auto id = static_cast<int32_t>(chat->id_);
        DEBUG(logger_, "[{}] adding id {} for {} to username cache", ctx_->id(), id, ctx_->ext_username);
        cache_->username_cache.emplace(ctx_->ext_username, id);
        dial_by_id(id);
    }

    void DialTg::parse_error(td_api::object_ptr<td::td_api::error> error) {
        std::smatch match;

        const std::regex delay_regex("Too Many Requests: retry after (\\d+)");
        if (std::regex_search(error->message_, match, delay_regex)) {
            auto seconds = std::stoi(match[1]);
            *block_until_ = std::chrono::steady_clock::now() +
                            std::chrono::seconds(seconds + settings_->extra_wait_time());
            return;
        }

        const std::regex flood_regex("PEER_FLOOD");
        if (std::regex_search(error->message_, match, flood_regex)) {
            *block_until_ = std::chrono::steady_clock::now() + std::chrono::seconds(settings_->peer_flood_time());
            return;
        }
    }

    void DialDtmf::operator()(Context &ctx, sip::Client &sip_client,
                              const td_api::object_ptr<td::td_api::updateNewMessage> &event,
                              std::shared_ptr<spdlog::logger> logger) const {

        auto content = td_api::move_object_as<td_api::messageText>(event->message_->content_);
        DEBUG(logger, "[{}] sending DTMF {}", ctx.id(), content->text_->text_);
        try {
            sip_client.DialDtmf(ctx.sip_call_id, content->text_->text_);
        } catch (const pj::Error &error) {
            logger->error(error.reason);
        }
    }
}

namespace state_machine {

    Logger::Logger(std::string context_id, shared_ptr<spdlog::logger> logger) : logger_(std::move(logger)),
                                                                                context_id_(std::move(context_id)) {
        TRACE(logger_, "[{}] logger created", context_id_);
    }

    Logger::~Logger() {
        TRACE(logger_, "[{}] ~logger", context_id_);
    };

    template<class SM>
    void Logger::log_process_event(const td::td_api::object_ptr<td::td_api::updateCall> &event) {
        TRACE(logger_, "[{}] [process_event]\n{}", context_id_, td::td_api::to_string(event));
    };

    template<class SM>
    void Logger::log_process_event(const sip::events::Event &event) {
        TRACE(logger_, "[{}] [process_event] sip", context_id_);
    };

    template<class SM, class TEvent>
    void Logger::log_process_event(const TEvent &) {
        TRACE(logger_, "[{}] [process_event] {}", context_id_, sml::aux::get_type_name<TEvent>());
    }

    template<class SM, class TGuard, class TEvent>
    void Logger::log_guard(const TGuard &, const TEvent &event, bool result) {
        TRACE(logger_, "[{}] [guard] {} {} {}", context_id_, sml::aux::get_type_name<TGuard>(),
              "event", (result ? "[OK]" : "[Reject]"));
    };

    template<class SM, class TAction, class TEvent>
    void Logger::log_action(const TAction &, const TEvent &event) {
        TRACE(logger_, "[{}] [action] {} {}", context_id_, sml::aux::get_type_name<TAction>(), "event");
    };

    template<class SmLogger, class TSrcState, class TDstState>
    void Logger::log_state_change(const TSrcState &src, const TDstState &dst) {
        TRACE(logger_, "[{}] [transition] {} -> {}", context_id_, src.c_str(), dst.c_str());
    }

    struct from_tg {
        auto operator()() const {
            using namespace boost::sml;
            using namespace td::td_api;
            using namespace guards;
            using namespace actions;
            return make_transition_table(
                    *"sip_wait_media"_s + event<sip::events::CallMediaStateUpdate>[IsMediaReady{}]
                                          / AnswerTg{} = "wait_tg"_s,
                    "wait_tg"_s + event<object_ptr<updateCall>>[IsInState{callStateReady::ID}]
                                  / (CreateTgVoip{}, BridgeAudio{}) = "wait_dtmf"_s,
                    "wait_dtmf"_s + event<object_ptr<updateNewMessage>>[IsTextContent{} && IsDtmfString{}]
                                  / DialDtmf{} = "wait_dtmf"_s
            );
        }
    };

    struct from_sip {
        auto operator()() const {
            using namespace boost::sml;
            using namespace td::td_api;
            using namespace guards;
            using namespace actions;
            return make_transition_table(
                    *"sip_wait_confirm"_s + event<sip::events::CallStateUpdate>
                                            [IsSipInState{PJSIP_INV_STATE_EARLY}] / DialTg{} = "wait_tg"_s,
                    "wait_tg"_s + event<object_ptr<updateCall>>[IsInState{callStateReady::ID}]
                                  / (StoreTgUserId{}, CreateTgVoip{}, AnswerSip{}) = "sip_wait_media"_s,
                    "sip_wait_media"_s + event<sip::events::CallMediaStateUpdate>[IsMediaReady{}]
                                         / BridgeAudio{} = "wait_dtmf"_s,
                    "wait_dtmf"_s + event<object_ptr<updateNewMessage>>[IsTextContent{} && IsDtmfString{}]
                                    / DialDtmf{} = "wait_dtmf"_s
            );
        }
    };

    struct StateMachine {
        auto operator()() const {
            using namespace boost::sml;
            using namespace td::td_api;
            using namespace guards;
            using namespace actions;
            using namespace events;
            return make_transition_table(
                    *"init"_s + event<object_ptr<updateCall>>
                                [IsIncoming{} && IsInState{callStatePending::ID} && CallbackUriIsSet{}]
                                / (StoreTgId{}, StoreTgUserId{}, DialSip{}) = state<from_tg>,
                    "init"_s + event<object_ptr<updateCall>>
                               [IsIncoming{} && IsInState{callStatePending::ID} && !CallbackUriIsSet{}]
                               / StoreTgId{} = X,
                    "init"_s + event<object_ptr<updateCall>> = X,
                    "init"_s + event<sip::events::IncomingCall> / (StoreSipId{}, AcceptIncomingSip{}) = state<from_sip>,
                    "init"_s + event<sip::events::CallStateUpdate> = X,
                    "init"_s + event<sip::events::CallMediaStateUpdate> = X,

                    state<from_tg> + event<object_ptr<updateCall>>
                                     [IsInState{callStateDiscarded::ID} || IsInState{callStateError::ID}] /
                                     CleanTgId{} = X,
                    state<from_tg> + event<sip::events::CallStateUpdate>
                                     [IsSipInState{PJSIP_INV_STATE_DISCONNECTED}] / CleanSipId{} = X,
                    state<from_tg> + event<InternalError> / SetHangupPrm{} = X,

                    state<from_sip> + event<object_ptr<updateCall>>
                                      [IsInState{callStateDiscarded::ID} || IsInState{callStateError::ID}] /
                                      CleanTgId{} = X,
                    state<from_sip> + event<InternalError> / SetHangupPrm{} = X,
                    state<from_sip> + event<sip::events::CallStateUpdate>
                                      [IsSipInState{PJSIP_INV_STATE_DISCONNECTED}] / CleanSipId{} = X,

                    X + on_entry<_> / CleanUp{}
            );
        }
    };
}

Context::Context() : id_(next_ctx_id()) {}

const std::string Context::id() const { return id_; };

std::string Context::next_ctx_id() {
    static int64_t ctx_counter{0};
    static std::string id_prefix = std::to_string(getpid()) + "-";
    return id_prefix + std::to_string(++ctx_counter);
}

Gateway::Gateway(sip::Client &sip_client_, tg::Client &tg_client_,
                 OptionalQueue<sip::events::Event> &sip_events_,
                 OptionalQueue<tg::Client::Object> &tg_events_,
                 std::shared_ptr<spdlog::logger> logger_,
                 Settings &settings)
        : sip_client_(sip_client_), tg_client_(tg_client_), logger_(std::move(logger_)),
          sip_events_(sip_events_), tg_events_(tg_events_), settings_(settings) {}

void Gateway::start() {

    load_cache();

    signal(SIGINT, [](int) { e_flag = 1; });
    signal(SIGTERM, [](int) { e_flag = 1; });

    while (!e_flag) {
        auto tick_start = std::chrono::steady_clock::now();

        if (auto event = internal_events_.pop(); event) {
            std::visit([this](auto &&casted_event) {
                process_event(casted_event);
            }, event.value());
        }

        if (auto event = tg_events_.pop(); event) {
            using namespace td::td_api;
            auto &&object = event.value();
            switch (object->get_id()) {
                case updateCall::ID:
                    process_event(move_object_as<updateCall>(object));
                    break;
                case updateNewMessage::ID:
                    process_event(move_object_as<updateNewMessage>(object));
                    break;
                default:
                    break;
            }
        }

        if (auto event = sip_events_.pop(); event) {
            std::visit([this](auto &&casted_event) {
                process_event(casted_event);
            }, event.value());
        }

        auto tick_end = std::chrono::steady_clock::now();
        auto duration = tick_end - tick_start;
        auto sleep_time = std::chrono::milliseconds(10) - duration;
        if (sleep_time.count() > 0) {
            std::this_thread::sleep_for(sleep_time);
        }
    }

}

void Gateway::load_cache() {
    logger_->info("Loading contacts cache");

    auto search_response = tg_client_.send_query_async(
            td::td_api::make_object<td::td_api::searchContacts>("", INT32_MAX)).get();
    if (search_response->get_id() == td::td_api::error::ID) {
        logger_->error("offline contacts search failed during cache fill\n{}", to_string(search_response));
        return;
    }

    auto users = td::td_api::move_object_as<td::td_api::users>(search_response);
    std::vector<std::future<tg::Client::Object>> responses;
    for (auto user_id : users->user_ids_) {
        responses.emplace_back(tg_client_.send_query_async(td::td_api::make_object<td::td_api::getUser>(user_id)));
    }
    for (auto &future : responses) {
        auto response = future.get();

        if (response->get_id() == td::td_api::error::ID) {
            logger_->error("failed to get user info from DB\n{}", to_string(response));
            continue;
        }

        auto user = td::td_api::move_object_as<td::td_api::user>(response);

        if (!user->have_access_) {
            return;
        }

        if (!user->username_.empty()) {
            cache_.username_cache.emplace(user->username_, user->id_);
        }

        if (!user->phone_number_.empty()) {
            cache_.phone_cache.emplace(user->phone_number_, user->id_);
        }
    }

    logger_->info("Loaded {} usernames and {} phones into contacts cache",
                  cache_.username_cache.size(),
                  cache_.phone_cache.size());
}

std::vector<Bridge *>::iterator Gateway::search_call(const std::function<bool(const Bridge *)> &predicate) {
    auto iter = std::find_if(bridges.begin(), bridges.end(), predicate);

    if (iter == bridges.end()) {
        auto ctx = std::make_unique<Context>();
        auto sm_logger = std::make_unique<state_machine::Logger>(ctx->id(), logger_);
        auto sm = std::make_unique<state_machine::sm_t>(*sm_logger, sip_client_, tg_client_, settings_, logger_,
                                                        *ctx, cache_, internal_events_, block_until);

        auto bridge = new Bridge;
        bridge->ctx = std::move(ctx);
        bridge->sm = std::move(sm);
        bridge->logger = std::move(sm_logger);

        bridges.emplace_back(bridge);
        iter = bridges.end() - 1;
    }

    return iter;
}

void Gateway::process_event(td::td_api::object_ptr<td::td_api::updateCall> update_call) {

    auto iter = search_call([id = update_call->call_->id_](const Bridge *bridge) {
        return bridge->ctx->tg_call_id == id;
    });

    (*iter)->sm->process_event(update_call);

    if ((*iter)->sm->is(sml::X)) {
        delete *iter;
        bridges.erase(iter);
    }
}

void Gateway::process_event(td::td_api::object_ptr<td::td_api::updateNewMessage> update_message) {

    std::vector<Bridge *> matches;
    for (auto bridge : bridges) {
        if (bridge->ctx->user_id == update_message->message_->sender_user_id_) {
            matches.emplace_back(bridge);
        }
    }

    if (matches.size() > 1) {
        logger_->error("ambiguous message from {}", update_message->message_->sender_user_id_);
        return;
    } else if (matches.size() == 1) {
        TRACE(logger_, "routing message to ctx {}", matches[0]->ctx->id());
        matches[0]->sm->process_event(update_message);
    }

}

void Gateway::process_event(state_machine::events::InternalError &event) {
    auto iter = std::find_if(bridges.begin(), bridges.end(), [id = event.ctx_id](const Bridge *bridge) {
        return bridge->ctx->id() == id;
    });

    if (iter == bridges.end()) {
        return;
    }

    (*iter)->sm->process_event(event);

    if ((*iter)->sm->is(sml::X)) {
        delete *iter;
        bridges.erase(iter);
    }

}

template<typename TSipEvent>
void Gateway::process_event(const TSipEvent &event) {

    auto iter = search_call([id = event.id](const Bridge *bridge) {
        return bridge->ctx->sip_call_id == id;
    });

    (*iter)->sm->process_event(event);

    if ((*iter)->sm->is(sml::X)) {
        delete *iter;
        bridges.erase(iter);
    }
}