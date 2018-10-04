#ifndef TG2SIP_SOFTWAREAUDIOOUTPUT_H
#define TG2SIP_SOFTWAREAUDIOOUTPUT_H

#include <pjsua2.hpp>
#include "AudioOutput.h"
#include "../threading.h"

namespace tgvoip {
    namespace audio {
        class SoftwareAudioOutput : public AudioOutput, public pj::AudioMedia {
        public:
            explicit SoftwareAudioOutput();

            virtual ~SoftwareAudioOutput();

            void Start() override;

            void Stop() override;

            virtual bool IsPlaying();

        private:
            static pj_status_t PutFrameCallback(pjmedia_port *port, pjmedia_frame *frame);

            static pj_status_t GetFrameCallback(pjmedia_port *port, pjmedia_frame *frame);

            bool isActive;

            pj_pool_t *pj_pool;
            pjmedia_port *media_port;

            bool read_buffer{true};
            unsigned char *buffer;
        };
    }
}

#endif //TG2SIP_SOFTWAREAUDIOOUTPUT_H