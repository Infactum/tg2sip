#include <cassert>
#include "SoftwareAudioInput.h"

using namespace tgvoip::audio;

SoftwareAudioInput::SoftwareAudioInput() {
    isActive = false;

    pj_pool = pjsua_pool_create("input%p", 2048, 512);
    media_port = PJ_POOL_ZALLOC_T(pj_pool, pjmedia_port);

    pj_status_t status;
    pj_str_t name = pj_str((char *) "input");

    status = pjmedia_port_info_init(&media_port->info,
                                    &name,
                                    PJMEDIA_SIG_CLASS_PORT_AUD('S', 'I'), // Software Input SIG
                                    48000, 1, 16, 960);
    assert(status == PJ_SUCCESS);

    media_port->port_data.pdata = this;
    media_port->put_frame = &PutFrameCallback;
    media_port->get_frame = &GetFrameCallback;

    registerMediaPort(media_port);
}

SoftwareAudioInput::~SoftwareAudioInput() {
    unregisterMediaPort();
    pjmedia_port_destroy(media_port);
    pj_pool_release(pj_pool);
}

void SoftwareAudioInput::Start() {
    isActive = true;
}

void SoftwareAudioInput::Stop() {
    isActive = false;
}

pj_status_t SoftwareAudioInput::PutFrameCallback(pjmedia_port *port, pjmedia_frame *frame) {

    // skip heartbeat frame
    if (frame->type != PJMEDIA_FRAME_TYPE_AUDIO) {
        return PJ_SUCCESS;
    }

    assert(frame->size == 960 * 2);

    auto input = (SoftwareAudioInput *) port->port_data.pdata;

    if (!input->isActive) {
        return PJ_SUCCESS;
    }

    input->InvokeCallback((unsigned char *) frame->buf, frame->size);

    return PJ_SUCCESS;
}

pj_status_t SoftwareAudioInput::GetFrameCallback(pjmedia_port *port, pjmedia_frame *frame) {
    frame->size = 0;
    frame->type = PJMEDIA_FRAME_TYPE_NONE;
    return PJ_SUCCESS;
}