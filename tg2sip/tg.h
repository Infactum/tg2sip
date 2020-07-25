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

#ifndef TG2SIP_TG_H
#define TG2SIP_TG_H

#include <thread>
#include <functional>
#include <future>
#include <map>
#include <td/telegram/Client.h>
#include "utils.h"
#include "logging.h"
#include "settings.h"
#include "queue.h"

namespace tg {

    namespace td_api = td::td_api;

    class Client {
    public:
        using Object = td_api::object_ptr<td_api::Object>;

        virtual ~Client();

        explicit Client(Settings &settings, std::shared_ptr<spdlog::logger> logger_,
                          OptionalQueue<Object> &events_);

        void start();

        void send_query(td_api::object_ptr<td_api::Function> f, std::function<void(Object)> handler = nullptr);

        std::future<Object> send_query_async(td_api::object_ptr<td_api::Function> f);

        std::future<bool> is_ready() { return is_ready_.get_future(); };

    private:

        std::shared_ptr<spdlog::logger> logger;
        OptionalQueue<Object> &events;

        const double WAIT_TIMEOUT = 10;

        std::unique_ptr<td::Client> client;
        td::tl::unique_ptr<td_api::tdlibParameters> lib_parameters;
        td_api::object_ptr<td_api::Function> set_proxy;

        std::thread thread_;

        std::promise<bool> is_ready_;
        bool is_closed{false};
        std::uint64_t current_query_id{0};

        std::map<std::uint64_t, std::function<void(Object)>> handlers;

        void init_lib_parameters(Settings &settings);

        void init_proxy(Settings &settings);

        void loop();

        void process_response(td::Client::Response response);

        void process_update(Object update);

        std::uint64_t next_query_id();

        auto create_authentication_query_handler();

        void check_authentication_error(Object object);

        void on_authorization_state_update(td_api::object_ptr<td_api::AuthorizationState> authorization_state);
    };
}

#endif //TG2SIP_TG_H
