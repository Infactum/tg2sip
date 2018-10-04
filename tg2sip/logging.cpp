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
#include <td/telegram/Log.h>
#include "logging.h"

void init_logging(Settings &settings) {

    try {
        std::vector<spdlog::sink_ptr> sinks;

        auto console_sink = std::make_shared<spdlog::sinks::ansicolor_stdout_sink_mt>();
        console_sink->set_level(static_cast<spdlog::level::level_enum>(settings.console_min_level()));
        sinks.push_back(std::move(console_sink));

        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>("tg2sip.log", 100 * 1024 * 1024, 1);
        file_sink->set_level(static_cast<spdlog::level::level_enum>(settings.file_min_level()));
        sinks.push_back(std::move(file_sink));

        std::vector<string> loggers;
        loggers.emplace_back("core");
        loggers.emplace_back("pjsip");
        loggers.emplace_back("tgvoip");

        for (auto log_name : loggers) {
            auto logger = std::make_shared<spdlog::logger>(log_name, begin(sinks), end(sinks));
            spdlog::register_logger(logger);
        }

#ifndef DISABLE_DEBUG_
        spdlog::set_pattern("%^[%T.%e][t:%t][p:%P][%n][%l] %v%$");
#endif

        spdlog::apply_all([&](std::shared_ptr<spdlog::logger> l) {
            l->set_level(spdlog::level::info);
        });

    }
    catch (const spdlog::spdlog_ex &ex) {
        std::cerr << ex.what() << std::endl;
    }

    td::Log::set_file_path("tdlib.log");
    td::Log::set_verbosity_level(settings.tdlib_log_level());

    spdlog::get("core")->set_level(static_cast<spdlog::level::level_enum>(settings.log_level()));
    spdlog::get("pjsip")->set_level(static_cast<spdlog::level::level_enum>(settings.pjsip_log_level()));
    spdlog::get("tgvoip")->set_level(static_cast<spdlog::level::level_enum>(settings.tgvoip_log_level()));

}