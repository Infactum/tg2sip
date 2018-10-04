#ifndef TG2SIP_SOFTWAREAUDIOINPUT_H
#define TG2SIP_SOFTWAREAUDIOINPUT_H

#include <pjsua2.hpp>
#include "AudioInput.h"
#include "../threading.h"

namespace tgvoip {
    namespace audio {
        class SoftwareAudioInput : public AudioInput, public pj::AudioMedia {
        public:
            explicit SoftwareAudioInput();

            virtual ~SoftwareAudioInput();

            void Start() override;

            void Stop() override;

        private:
            static pj_status_t PutFrameCallback(pjmedia_port *port, pjmedia_frame *frame);

            static pj_status_t GetFrameCallback(pjmedia_port *port, pjmedia_frame *frame);

            bool isActive;

            pj_pool_t *pj_pool;
            pjmedia_port *media_port;
        };
    }
}

#endif //TG2SIP_SOFTWAREAUDIOINPUT_H