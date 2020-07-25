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
#include <INIReader.h>
#include <td/telegram/Client.h>
#include <td/telegram/Log.h>
#include <functional>
#include <map>
#include "settings.h"

// overloaded
namespace detail {
    template<class... Fs>
    struct overload;

    template<class F>
    struct overload<F> : public F {
        explicit overload(F f) : F(f) {
        }
    };

    template<class F, class... Fs>
    struct overload<F, Fs...>
            : public overload<F>, overload<Fs...> {
        overload(F f, Fs... fs) : overload<F>(f), overload<Fs...>(fs...) {
        }

        using overload<F>::operator();
        using overload<Fs...>::operator();
    };
}  // namespace detail

template<class... F>
auto overloaded(F... f) {
    return detail::overload<F...>(f...);
}

namespace td_api = td::td_api;

class TDClient {
public:
    explicit TDClient(Settings &settings_) : settings(settings_) {
        td::Log::set_verbosity_level(1);
        client = std::make_unique<td::Client>();
    }

    void auth() {
        while (!sequence_done) {
            process_response(client->receive(10));
        }
    }

private:
    using Object = td_api::object_ptr<td_api::Object>;

    Settings &settings;
    bool sequence_done{false};

    std::unique_ptr<td::Client> client;
    td_api::object_ptr<td_api::AuthorizationState> authorization_state;

    std::uint64_t current_query_id{0};
    std::uint64_t authentication_query_id{0};
    std::map<std::uint64_t, std::function<void(Object)>> handlers;

    std::uint64_t next_query_id() {
        return ++current_query_id;
    }

    void send_query(td_api::object_ptr<td_api::Function> f, std::function<void(Object)> handler) {
        auto query_id = next_query_id();
        if (handler) {
            handlers.emplace(query_id, std::move(handler));
        }
        client->send({query_id, std::move(f)});
    }

    void process_response(td::Client::Response response) {
        if (!response.object) {
            return;
        }

        if (response.id == 0) {
            return process_update(std::move(response.object));
        }

        auto it = handlers.find(response.id);
        if (it != handlers.end()) {
            it->second(std::move(response.object));
            handlers.erase(it->first);
        }
    }

    void process_update(td_api::object_ptr<td_api::Object> update) {
        td_api::downcast_call(
                *update, overloaded(
                        [this](td_api::updateAuthorizationState &update_authorization_state) {
                            authorization_state = std::move(update_authorization_state.authorization_state_);
                            on_authorization_state_update();
                        },
                        [](auto &update) {}));
    }

    auto create_authentication_query_handler() {
        return [this, id = authentication_query_id](Object object) {
            if (id == authentication_query_id) {
                check_authentication_error(std::move(object));
            }
        };
    }

    void check_authentication_error(Object object) {
        if (object->get_id() == td_api::error::ID) {
            auto error = td::move_tl_object_as<td_api::error>(object);
            std::cerr << "Error: " << to_string(error);
            on_authorization_state_update();
        }
    }

    void on_authorization_state_update() {
        authentication_query_id++;
        td_api::downcast_call(
                *authorization_state,
                overloaded(
                        [this](td_api::authorizationStateReady &) {
                            sequence_done = true;
                            std::cerr << "Authorization OK" << std::endl;
                        },
                        [this](td_api::authorizationStateLoggingOut &) {
                            sequence_done = true;
                            std::cerr << "Logging out" << std::endl;
                        },
                        [this](td_api::authorizationStateClosing &) {
                            std::cerr << "Closing" << std::endl;
                        },
                        [this](td_api::authorizationStateClosed &) {
                            sequence_done = true;
                            std::cerr << "Terminated" << std::endl;
                        },
                        [this](td_api::authorizationStateWaitCode &) {
                            std::cout << "Enter authentication code: " << std::flush;
                            std::string code;
                            std::cin >> code;
                            send_query(td_api::make_object<td_api::checkAuthenticationCode>(code),
                                       create_authentication_query_handler());
                        },
                        [this](td_api::authorizationStateWaitRegistration &) {
                            std::string first_name;
                            std::string last_name;
                            std::cout << "Enter your first name: " << std::flush;
                            std::cin >> first_name;
                            std::cout << "Enter your last name: " << std::flush;
                            std::cin >> last_name;
                            send_query(td_api::make_object<td_api::registerUser>(first_name, last_name),
                                       create_authentication_query_handler());
                        },
                        [this](td_api::authorizationStateWaitPassword &) {
                            std::cerr << "Enter authentication password: ";
                            std::string password;
                            std::cin >> password;
                            send_query(td_api::make_object<td_api::checkAuthenticationPassword>(password),
                                       create_authentication_query_handler());
                        },
                        [this](td_api::authorizationStateWaitOtherDeviceConfirmation &state) {
                            std::cout << "Confirm this login link on another device: " << state.link_ << std::endl;
                        },
                        [this](td_api::authorizationStateWaitPhoneNumber &) {
                            std::cout << "Enter phone number: " << std::flush;
                            std::string phone_number;
                            std::cin >> phone_number;
                            send_query(td_api::make_object<td_api::setAuthenticationPhoneNumber>(phone_number, nullptr),
                                       create_authentication_query_handler());
                        },
                        [this](td_api::authorizationStateWaitEncryptionKey &) {
                            send_query(td_api::make_object<td_api::checkDatabaseEncryptionKey>(""),
                                       create_authentication_query_handler());

                            if (settings.proxy_enabled()) {
                                auto socks_proxy_type = td_api::make_object<td_api::proxyTypeSocks5>(
                                        settings.proxy_username(),
                                        settings.proxy_password()
                                );
                                send_query(td_api::make_object<td_api::addProxy>(
                                        settings.proxy_address(),
                                        settings.proxy_port(),
                                        true,
                                        td_api::move_object_as<td_api::ProxyType>(socks_proxy_type)),
                                           [](Object) {});
                            } else {
                                send_query(td_api::make_object<td_api::disableProxy>(), [](Object) {});
                            }

                        },
                        [this](td_api::authorizationStateWaitTdlibParameters &) {
                            auto lib_parameters = td_api::make_object<td_api::tdlibParameters>();
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

                            send_query(td_api::make_object<td_api::setTdlibParameters>(std::move(lib_parameters)),
                                       create_authentication_query_handler());
                        }));
    }
};

int main() {

    auto reader = INIReader("settings.ini");
    Settings settings(reader);

    if (!settings.is_loaded()) {
        return 1;
    }

    TDClient client(settings);
    client.auth();
}