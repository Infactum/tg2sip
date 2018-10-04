#include <cassert>
#include "SoftwareAudioOutput.h"

using namespace tgvoip::audio;

SoftwareAudioOutput::SoftwareAudioOutput() {
    isActive = false;

    pj_pool = pjsua_pool_create("output%p", 2048, 512);
    media_port = PJ_POOL_ZALLOC_T(pj_pool, pjmedia_port);

    pj_status_t status;
    pj_str_t name = pj_str((char *) "output");

    // ptime = 10ms -> 480 samples per frame
    status = pjmedia_port_info_init(&media_port->info,
                                    &name,
                                    PJMEDIA_SIG_CLASS_PORT_AUD('S', 'O'), // Software Output SIG
                                    48000, 1, 16, 480);
    assert(status == PJ_SUCCESS);

    media_port->port_data.pdata = this;
    media_port->put_frame = &PutFrameCallback;
    media_port->get_frame = &GetFrameCallback;

    registerMediaPort(media_port);

    // 48kHz * 20 ms * 16 bits per sample
    buffer = new unsigned char[1920];
}

SoftwareAudioOutput::~SoftwareAudioOutput() {
    unregisterMediaPort();
    pjmedia_port_destroy(media_port);
    pj_pool_release(pj_pool);
    if (buffer) {
        delete buffer;
    }
}

void SoftwareAudioOutput::Start() {
    isActive = true;
}

void SoftwareAudioOutput::Stop() {
    isActive = false;
}

bool SoftwareAudioOutput::IsPlaying() {
    return isActive;
}

pj_status_t SoftwareAudioOutput::PutFrameCallback(pjmedia_port *port, pjmedia_frame *frame) {

    frame->size = 0;
    frame->type = PJMEDIA_FRAME_TYPE_NONE;
    return PJ_SUCCESS;
}

pj_status_t SoftwareAudioOutput::GetFrameCallback(pjmedia_port *port, pjmedia_frame *frame) {

    auto output = (SoftwareAudioOutput *) port->port_data.pdata;

    if (!output->isActive) {
        frame->type = PJMEDIA_FRAME_TYPE_NONE;
        frame->size = 0;
        return PJ_SUCCESS;
    }

    // libtgvoip ptime = 20ms
    if (output->read_buffer) {
        output->InvokeCallback(output->buffer, 1920);
        memcpy(frame->buf, output->buffer, 960);
    } else {
        memcpy(frame->buf, output->buffer + 960, 960);
    }

    output->read_buffer = !output->read_buffer;

    return PJ_SUCCESS;
}
