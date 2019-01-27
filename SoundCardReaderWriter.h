/*
 *	Copyright (C) 2009,2010,2015,2018 by Jonathan Naylor, G4KLX
 *	Copyright (C) 2014 by John Wiseman, G8BPQ
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; version 2 of the License.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 */

#ifndef	SoundCardReaderWriter_H
#define	SoundCardReaderWriter_H

#include <string>
#include <pulse/pulseaudio.h>

#include "AudioCallback.h"

class CSoundCardReaderWriter {
public:
	CSoundCardReaderWriter(const std::string& captureDevice, const std::string& playbackDevice, unsigned int sampleRate, unsigned int blockSize);
	~CSoundCardReaderWriter();

	void setCallback(IAudioCallback* callback);
	bool open();
	void close();

	bool isWriterBusy() const;

	static void contextStateCallback(pa_context *c, void *userdata);
	static void streamWriteCallback(pa_stream* stream, size_t bytes, void* userdata);
	static void streamReadCallback(pa_stream* stream, size_t bytes, void* userdata);

    static void* receiveProcessingLoop(void *userdata);

private:
	std::string             m_captureDevice;
	std::string             m_playbackDevice;
	unsigned int            m_sampleRate;
	unsigned int            m_blockSize;
	IAudioCallback*         m_callback;
	pa_threaded_mainloop*   m_mainloop;
	pa_context*             m_context;
	pa_stream*              m_playbackStream;
	pa_stream*              m_captureStream;

    pthread_t             m_receiveThread;

	float                   m_writeBuffer[96000];
};

#endif
