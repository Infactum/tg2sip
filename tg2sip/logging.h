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

#ifndef TG2SIP_LOGGING_H
#define TG2SIP_LOGGING_H

#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>
#include "settings.h"

#ifndef DISABLE_DEBUG_
#define STRINGIZE_(x) #x
#define STRINGIZE(x) STRINGIZE_(x)
#define TRACE(logger, ...) logger->trace("" __FILE__  ":" STRINGIZE(__LINE__)" " __VA_ARGS__)
#define DEBUG(logger, ...) logger->debug(__VA_ARGS__)
#else
#define TRACE(logger, ...) (void)0
#define DEBUG(logger, ...) (void)0
#endif

void init_logging(Settings &settings);

#endif //TG2SIP_LOGGING_H
