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

#ifndef TG2SIP_QUEUE_H
#define TG2SIP_QUEUE_H

#include <mutex>
#include <queue>
#include <condition_variable>

template<typename T>
class OptionalQueue {
public:
    OptionalQueue() = default;

    OptionalQueue(const OptionalQueue &) = delete;

    OptionalQueue &operator=(const OptionalQueue &) = delete;

    virtual ~OptionalQueue() = default;

    void emplace(std::optional<T> &&value) {
        {
            std::unique_lock<std::mutex> lock(this->mutex);
            q.emplace(std::move(value));
        }
    };

    std::optional<T> pop() {
        std::unique_lock<std::mutex> lock(this->mutex);

        if (q.empty()) {
            return std::nullopt;
        }

        std::optional<T> value = std::move(this->q.front());
        this->q.pop();

        return value;
    };

private:
    std::queue<std::optional<T>> q;
    std::mutex mutex;
};

#endif //TG2SIP_QUEUE_H
