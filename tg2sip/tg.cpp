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

#include "tg.h"
#include "queue.h"

using namespace tg;

Client::Client(Settings &settings, std::shared_ptr<spdlog::logger> logger_,
               OptionalQueue<Object> &events_)
        : logger(std::move(logger_)), events(events_) {

    client = std::make_unique<td::Client>();
    init_lib_parameters(settings);
    init_proxy(settings);
}

Client::~Client() {
    TRACE(logger, "~Client");
    is_closed = true;
    if (thread_.joinable()) {
        thread_.join();
    }
    client.release();
    TRACE(logger, "~Client done");
}

void Client::init_lib_parameters(Settings &settings) {
    lib_parameters = td_api::make_object<td_api::tdlibParameters>();

    lib_parameters->api_id_ = settings.api_id();
    lib_parameters->api_hash_ = settings.api_hash();
    lib_parameters->database_directory_ = settings.db_folder();

    lib_parameters->system_language_code_ = settings.sys_lang_code();
    lib_parameters->device_model_ = settings.device_model();
    lib_parameters->system_version_ = settings.system_version();
    lib_parameters->application_version_ = settings.app_version();

    lib_parameters->use_file_database_ = false;
    lib_parameters->use_chat_info_database_ = true;
    lib_parameters->use_message_database_ = false;
    lib_parameters->use_secret_chats_ = false;
    lib_parameters->enable_storage_optimizer_ = true;
}

void Client::init_proxy(Settings &settings) {
    if (settings.proxy_enabled()) {
        auto socks_proxy_type = td_api::make_object<td_api::proxyTypeSocks5>(
                settings.proxy_username(),
                settings.proxy_password()
        );
        set_proxy = td_api::make_object<td_api::addProxy>(
                settings.proxy_address(),
                settings.proxy_port(),
                true,
                td_api::move_object_as<td_api::ProxyType>(socks_proxy_type));
    } else {
        set_proxy = td_api::make_object<td_api::disableProxy>();
    }
}

void Client::loop() {
    TRACE(logger, "TG client thread started");

    while (!is_closed) {
        auto response = client->receive(WAIT_TIMEOUT);
        process_response(std::move(response));
    }
    TRACE(logger, "TG client thread ended");
}

void Client::start() {
    thread_ = std::thread(&Client::loop, this);
    pthread_setname_np(thread_.native_handle(), "tg_client");
}

void Client::process_response(td::Client::Response response) {

    if (!response.object) {
        return;
    }

    TRACE(logger, "TG client got response with ID {}\n{}", response.id, to_string(response.object));

    if (response.id == 0) {
        return process_update(std::move(response.object));
    }

    auto it = handlers.find(response.id);
    if (it != handlers.end()) {
        it->second(std::move(response.object));
        handlers.erase(it->first);
    }
}

void Client::process_update(Object update) {
    switch (update->get_id()) {
        case td_api::updateAuthorizationState::ID: {
            auto update_authorization_state = td_api::move_object_as<td_api::updateAuthorizationState>(update);
            on_authorization_state_update(std::move(update_authorization_state->authorization_state_));
            break;
        }
        case td_api::updateConnectionState::ID: {
            auto update_connection_state = td_api::move_object_as<td_api::updateConnectionState>(update);
            if (update_connection_state->state_->get_id() == td_api::connectionStateReady::ID) {
                logger->info("TG client connected");
            }
            break;
        }
        case td_api::updateCall::ID:
        case td_api::updateNewMessage::ID: {
            events.emplace(std::move(update));
            break;
        }
        default:
            break;
    }
}

void Client::send_query(td_api::object_ptr<td_api::Function> f, std::function<void(Object)> handler) {
    auto query_id = next_query_id();
    if (handler) {
        handlers.emplace(query_id, std::move(handler));
    }
    client->send({query_id, std::move(f)});
}

std::future<Client::Object> Client::send_query_async(td_api::object_ptr<td_api::Function> f) {

    if (std::this_thread::get_id() == thread_.get_id()) {
        logger->critical("Call of send_query_async from TG thread will cause deadlock");
        throw;
    }

    auto promise = std::make_shared<std::promise<Object>>();
    auto future = promise->get_future();

    send_query(std::move(f), [this, promise](Object object) {
        try {
            promise->set_value(std::move(object));
        } catch (std::future_error &error) {
            logger->critical("failed to set send_query_async promise value {}", error.code());
        }
    });

    return future;
}

std::uint64_t Client::next_query_id() {
    return ++current_query_id;
}

auto Client::create_authentication_query_handler() {
    return [this](Object object) {
        check_authentication_error(std::move(object));
    };
}

void Client::check_authentication_error(Object object) {
    if (object->get_id() == td_api::error::ID) {
        auto error = td_api::move_object_as<td_api::error>(object);
        logger->error("TG client authorization error\n{}", to_string(error));
    }
}

void Client::on_authorization_state_update(td_api::object_ptr<td_api::AuthorizationState> authorization_state) {
    switch (authorization_state->get_id()) {
        case td_api::authorizationStateReady::ID:
            is_ready_.set_value(true);
            logger->info("TG client authorization ready");
            break;
        case td_api::authorizationStateWaitEncryptionKey::ID:
            send_query(td_api::make_object<td_api::checkDatabaseEncryptionKey>(),
                       create_authentication_query_handler());
            send_query(std::move(set_proxy), create_authentication_query_handler());
            break;
        case td_api::authorizationStateWaitTdlibParameters::ID: {
            send_query(td_api::make_object<td_api::setTdlibParameters>(
                    std::move(lib_parameters)), create_authentication_query_handler());
            break;
        }
        default:
            is_ready_.set_value(false);
            logger->error("TG client auto sign-on failed");
            break;
    }
}
