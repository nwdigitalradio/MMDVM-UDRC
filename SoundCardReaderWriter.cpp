/*
 *   Copyright (C) 2006-2010,2015,2018 by Jonathan Naylor G4KLX
 *   Copyright (C) 2014 by John Wiseman, G8BPQ
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "SoundCardReaderWriter.h"

#include <cstdio>
#include <cassert>
#include <pulse/pulseaudio.h>
#include <string.h>


CSoundCardReaderWriter::CSoundCardReaderWriter(const std::string& captureDevice, const std::string& playbackDevice, unsigned int sampleRate, unsigned int blockSize) :
m_captureDevice(captureDevice),
m_playbackDevice(playbackDevice),
m_sampleRate(sampleRate),
m_blockSize(blockSize),
m_callback(NULL),
m_mainloop(NULL) {
    assert(sampleRate > 0U);
    assert(blockSize > 0U);
}

CSoundCardReaderWriter::~CSoundCardReaderWriter() {
}

void CSoundCardReaderWriter::setCallback(IAudioCallback* callback) {
	assert(callback != NULL);

	m_callback = callback;
}

void CSoundCardReaderWriter::contextStateCallback(pa_context *c, void *userdata) {
    CSoundCardReaderWriter *This = static_cast<CSoundCardReaderWriter*>(userdata);

    switch(pa_context_get_state(c)) {
        case PA_CONTEXT_READY:
        case PA_CONTEXT_TERMINATED:
        case PA_CONTEXT_FAILED:
            pa_threaded_mainloop_signal(This->m_mainloop, 0);
            break;
        case PA_CONTEXT_UNCONNECTED:
        case PA_CONTEXT_CONNECTING:
        case PA_CONTEXT_AUTHORIZING:
        case PA_CONTEXT_SETTING_NAME:
            break;
    }
}

void CSoundCardReaderWriter::streamWriteCallback(pa_stream* stream, size_t bytes, void* userdata) {
    CSoundCardReaderWriter *This = static_cast<CSoundCardReaderWriter*>(userdata);
    char *buffer;

    //    fprintf(stderr, "Request to write %d bytes\n", bytes);
    // XXX Need to fill buffer here.
    buffer = (char *) malloc(bytes);
    ::memset(buffer, 0, bytes);
    pa_stream_write(stream, buffer, bytes, NULL, 0, PA_SEEK_RELATIVE);
//    ::fprintf(stderr, ".");
    free(buffer);
}

void CSoundCardReaderWriter::streamReadCallback(pa_stream* stream, size_t bytes, void *userdata) {
    CSoundCardReaderWriter *This = static_cast<CSoundCardReaderWriter*>(userdata);
    const void *data;

    while(pa_stream_readable_size(stream) > 0 ) {
        if(pa_stream_peek(stream, &data, &bytes) < 0) {
            ::fprintf(stderr, "pa_stream_peak() failed: %s", pa_strerror(pa_context_errno(This->m_context)));
            return;
        }
        This->m_callback->readCallback((float *) data, bytes / sizeof(float));
        pa_stream_drop(stream);
    }

    pa_threaded_mainloop_signal(This->m_mainloop, 0);

//    ::fprintf(stderr, "!");
}

void* CSoundCardReaderWriter::receiveProcessingLoop(void *userdata) {
    CSoundCardReaderWriter* This = static_cast<CSoundCardReaderWriter*>(userdata);

    while(1) {
        pa_threaded_mainloop_lock(This->m_mainloop);
        pa_threaded_mainloop_wait(This->m_mainloop);
        This->m_callback->processReceiveData();
        pa_threaded_mainloop_unlock(This->m_mainloop);
    }
}

bool CSoundCardReaderWriter::open() {
    pa_sample_spec sampleSpec = {
        .format = PA_SAMPLE_FLOAT32NE,
        .rate = m_sampleRate,
        .channels = 1
    };


    pa_buffer_attr bufferAttr;
    bufferAttr.maxlength = -1;
    bufferAttr.tlength = m_blockSize * sizeof(float);
    bufferAttr.prebuf = -1;
    bufferAttr.minreq = -1;
    bufferAttr.fragsize = m_blockSize * sizeof(float);

    m_mainloop = pa_threaded_mainloop_new();
    assert(m_mainloop);

    pa_threaded_mainloop_lock(m_mainloop);

    m_context = pa_context_new(pa_threaded_mainloop_get_api(m_mainloop), "MMDVM-UDRC");
    if(!m_context) {
        ::fprintf(stderr, "Cannot create pulseaudio context.\n");
        pa_threaded_mainloop_unlock(m_mainloop);
        pa_threaded_mainloop_free(m_mainloop);
        return false;
    }

    pa_context_set_state_callback(m_context, contextStateCallback, this);

    if(pa_context_connect(m_context, "unix:/var/run/pulse/native", (pa_context_flags_t)0, NULL) < 0) {
        ::fprintf(stderr, "Cannot connect to pulseaudio context.\n");
        pa_context_disconnect(m_context);
        pa_context_unref(m_context);
        pa_threaded_mainloop_unlock(m_mainloop);
        pa_threaded_mainloop_free(m_mainloop);
        return false;
    }

    if(pa_threaded_mainloop_start(m_mainloop) < 0) {
        ::fprintf(stderr, "Cannot connect to start pulseaudio main loop.\n");
        pa_context_disconnect(m_context);
        pa_context_unref(m_context);
        pa_threaded_mainloop_unlock(m_mainloop);
        pa_threaded_mainloop_free(m_mainloop);
        return false;
    }

    ::fprintf(stderr, "Connecting to pulseaudio server...\n");
    pa_threaded_mainloop_wait(m_mainloop);
    if(pa_context_get_state(m_context) != PA_CONTEXT_READY) {
        ::fprintf(stderr, "Cannot connect to pulseaudio server.\n");
        goto error;
    }
    ::fprintf(stderr, "Connected to pulseaudio server.\n");

    m_playbackStream = pa_stream_new(m_context, "Transmit", &sampleSpec, NULL);
    if(!m_playbackStream) {
        ::fprintf(stderr, "Cannot create transmit stream\n");
        goto error;
    }

    m_captureStream = pa_stream_new(m_context, "Receive", &sampleSpec, NULL);
    if(!m_captureStream) {
        ::fprintf(stderr, "Cannot create receive stream\n");
        goto error;
    }

    pa_stream_set_write_callback(m_playbackStream, streamWriteCallback, this);
    pa_stream_set_read_callback(m_captureStream, streamReadCallback, this);

    // XXX Second parameter "dev" should be set to draws-left or draws-right
    if(pa_stream_connect_playback(m_playbackStream, m_playbackDevice.c_str(), &bufferAttr, static_cast<pa_stream_flags_t>(PA_STREAM_DONT_MOVE|PA_STREAM_ADJUST_LATENCY), NULL, NULL) < 0) {
        ::fprintf(stderr, "Cannot connect to pulseaudio playback stream\n");
        goto error;
    }

    // XXX Code to wait until stream is connected

    // XXX Second parameter "dev" should be set to draws-left or draws-right
    if(pa_stream_connect_record(m_captureStream, m_captureDevice.c_str(), &bufferAttr, static_cast<pa_stream_flags_t>(PA_STREAM_DONT_MOVE|PA_STREAM_ADJUST_LATENCY)) < 0) {
        ::fprintf(stderr, "Cannot connect to pulseaudio capture stream\n");
        goto error;
    }

    // XXX Code to wait until stream is connected

    pa_threaded_mainloop_unlock(m_mainloop);

    //  Start the receive processing thread
    pthread_create(&m_receiveThread, NULL, receiveProcessingLoop, this);

    return true;

    error:
        fprintf(stderr, "PulseAudio error: %s\n" ,pa_strerror(pa_context_errno(m_context)));
        pa_threaded_mainloop_unlock(m_mainloop);
        pa_threaded_mainloop_stop(m_mainloop);
        pa_context_disconnect(m_context);
        pa_context_unref(m_context);
        pa_threaded_mainloop_free(m_mainloop);
        return false;
}

void CSoundCardReaderWriter::close() {
}
