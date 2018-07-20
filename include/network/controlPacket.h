/*
 * File: include/network/controlPacket.h
 * Date: 20.07.2018
 *
 * MIT License
 *
 * Copyright (c) 2018 JustAnotherVoiceChat
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#pragma once

#include "networkPacket.h"

namespace javic {
    typedef enum {
        CONTROL_PACKET_TYPE_UNKNOWN = 0,
    } controlPacketType_t;

    class ControlPacket : public NetworkPacket {
    private:
        std::string _nickname;

    public:
        ControlPacket() : NetworkPacket(CONTROL_PACKET_TYPE_UNKNOWN) {
            _nickname = "";
        }

        ControlPacket(controlPacketType_t type) : NetworkPacket(type) {
            _nickname = "";
        }

        enet_uint8 channel() const override {
            return PACKET_CHANNEL_CONTROL;
        }

        void setNickname(std::string nickname) {
            _nickname = nickname;
        }

        std::string nickname() const {
            return _nickname;
        }

        friend class cereal::access;

        template <class Archive>
        void serialize(Archive &ar) {
            ar(CEREAL_NVP(_type),
               CEREAL_NVP(_nickname)
            );
        }
    };
}
