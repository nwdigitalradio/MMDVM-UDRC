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

CSoundCardReaderWriter::CSoundCardReaderWriter(const std::string& readDevice, const std::string& writeDevice, unsigned int sampleRate, unsigned int blockSize) :
m_readDevice(readDevice),
m_writeDevice(writeDevice),
m_sampleRate(sampleRate),
m_blockSize(blockSize),
m_callback(NULL),
m_reader(NULL),
m_writer(NULL)
{
    assert(sampleRate > 0U);
    assert(blockSize > 0U);
}

CSoundCardReaderWriter::~CSoundCardReaderWriter()
{
}

void CSoundCardReaderWriter::setCallback(IAudioCallback* callback)
{
	assert(callback != NULL);

	m_callback = callback;
}

bool CSoundCardReaderWriter::open()
{
	int err = 0;

	char buf1[100];
	char buf2[100];
	char* ptr;

	::strcpy(buf1, (const char*)m_writeDevice.c_str());
	::strcpy(buf2, (const char*)m_readDevice.c_str());

	ptr = ::strchr(buf1, ' ');
	if (ptr) *ptr = 0;				// Get Device part of name

	ptr = ::strchr(buf2, ' ');
	if (ptr) *ptr = 0;				// Get Device part of name

	std::string writeDevice(buf1);
	std::string readDevice(buf2);

	snd_pcm_t* playHandle = NULL;
	if ((err = ::snd_pcm_open(&playHandle, buf1, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
		::fprintf(stderr, "Cannot open playback audio device %s (%s)\n", writeDevice.c_str(), ::snd_strerror(err));
		return false;
	}

	snd_pcm_hw_params_t* hw_params;
	if ((err = ::snd_pcm_hw_params_malloc(&hw_params)) < 0) {
		::fprintf(stderr, "Cannot allocate hardware parameter structure (%s)\n", ::snd_strerror(err));
		return false;
	}

	if ((err = ::snd_pcm_hw_params_any(playHandle, hw_params)) < 0) {
		::fprintf(stderr, "Cannot initialize hardware parameter structure (%s)\n", ::snd_strerror(err));
		return false;
	}

	if ((err = ::snd_pcm_hw_params_set_access(playHandle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
		::fprintf(stderr, "Cannot set access type (%s)\n", ::snd_strerror(err));
		return false;
	}

	if ((err = ::snd_pcm_hw_params_set_format(playHandle, hw_params, SND_PCM_FORMAT_S16_LE)) < 0) {
		::fprintf(stderr, "Cannot set sample format (%s)\n", ::snd_strerror(err));
		return false;
	}

	if ((err = ::snd_pcm_hw_params_set_rate(playHandle, hw_params, m_sampleRate, 0)) < 0) {
		::fprintf(stderr, "Cannot set sample rate (%s)\n", ::snd_strerror(err));
		return false;
	}

	unsigned int playChannels = 1U;

	if ((err = ::snd_pcm_hw_params_set_channels(playHandle, hw_params, 1)) < 0) {
		playChannels = 2U;

		if ((err = ::snd_pcm_hw_params_set_channels(playHandle, hw_params, 2)) < 0) {
			::fprintf(stderr, "Cannot play set channel count (%s)\n", ::snd_strerror(err));
			return false;
		}
	}

	if ((err = ::snd_pcm_hw_params(playHandle, hw_params)) < 0) {
		::fprintf(stderr, "Cannot set parameters (%s)\n", ::snd_strerror(err));
		return false;
	}

	::snd_pcm_hw_params_free(hw_params);

	if ((err = ::snd_pcm_prepare(playHandle)) < 0) {
		::fprintf(stderr, "Cannot prepare audio interface for use (%s)\n", ::snd_strerror(err));
		return false;
	}

	// Open Capture
	snd_pcm_t* recHandle = NULL;
	if ((err = ::snd_pcm_open(&recHandle, buf2, SND_PCM_STREAM_CAPTURE, 0)) < 0) {
		::fprintf(stderr, "Cannot open capture audio device %s (%s)\n", readDevice.c_str(), ::snd_strerror(err));
		return false;
	}

	if ((err = ::snd_pcm_hw_params_malloc(&hw_params)) < 0) {
		::fprintf(stderr, "Cannot allocate hardware parameter structure (%s)\n", ::snd_strerror(err));
		return false;
	}

	if ((err = ::snd_pcm_hw_params_any(recHandle, hw_params)) < 0) {
		::fprintf(stderr, "Cannot initialize hardware parameter structure (%s)\n", ::snd_strerror(err));
		return false;
	}

	if ((err = ::snd_pcm_hw_params_set_access(recHandle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
		::fprintf(stderr, "Cannot set access type (%s)\n", ::snd_strerror(err));
		return false;
	}

	if ((err = ::snd_pcm_hw_params_set_format(recHandle, hw_params, SND_PCM_FORMAT_S16_LE)) < 0) {
		::fprintf(stderr, "Cannot set sample format (%s)\n", ::snd_strerror(err));
		return false;
	}

	if ((err = ::snd_pcm_hw_params_set_rate(recHandle, hw_params, m_sampleRate, 0)) < 0) {
		::fprintf(stderr, "Cannot set sample rate (%s)\n", ::snd_strerror(err));
		return false;
	}

	unsigned int recChannels = 1U;

	if ((err = ::snd_pcm_hw_params_set_channels(recHandle, hw_params, 1)) < 0) {
		recChannels = 2U;

		if ((err = ::snd_pcm_hw_params_set_channels (recHandle, hw_params, 2)) < 0) {
			::fprintf(stderr, "Cannot rec set channel count (%s)\n", ::snd_strerror(err));
			return false;
		}
	}

	if ((err = ::snd_pcm_hw_params(recHandle, hw_params)) < 0) {
		::fprintf(stderr, "Cannot set parameters (%s)\n", ::snd_strerror(err));
		return false;
	}

	::snd_pcm_hw_params_free(hw_params);

	if ((err = ::snd_pcm_prepare(recHandle)) < 0) {
		::fprintf(stderr, "Cannot prepare audio interface for use (%s)\n", ::snd_strerror(err));
		return false;
	}

	short samples[256];
	for (unsigned int i = 0U; i < 10U; ++i)
		::snd_pcm_readi(recHandle, samples, 128);

	::printf("Opened %s %s Rate %u\n", writeDevice.c_str(), readDevice.c_str(), m_sampleRate);

	m_reader = new CSoundCardReader(recHandle,  m_blockSize, recChannels,  m_callback);
	m_writer = new CSoundCardWriter(playHandle, m_blockSize, playChannels, m_callback);

	m_reader->run();
	m_writer->run();

 	return true;
}

void CSoundCardReaderWriter::close()
{
	m_reader->kill();
	m_writer->kill();

	m_reader->wait();
	m_writer->wait();
}

bool CSoundCardReaderWriter::isWriterBusy() const
{
	return m_writer->isBusy();
}

CSoundCardReader::CSoundCardReader(snd_pcm_t* handle, unsigned int blockSize, unsigned int channels, IAudioCallback* callback) :
CThread(),
m_handle(handle),
m_blockSize(blockSize),
m_channels(channels),
m_callback(callback),
m_killed(false),
m_buffer(NULL),
m_samples(NULL)
{
	assert(handle != NULL);
	assert(blockSize > 0U);
	assert(channels == 1U || channels == 2U);
	assert(callback != NULL);

	m_buffer  = new float[blockSize];
	m_samples = new short[2U * blockSize];
}

CSoundCardReader::~CSoundCardReader()
{
	delete[] m_buffer;
	delete[] m_samples;
}

void CSoundCardReader::entry()
{
	while (!m_killed) {
		snd_pcm_sframes_t ret;
		while ((ret = ::snd_pcm_readi(m_handle, m_samples, m_blockSize)) < 0) {
			if (ret != -EPIPE)
				::fprintf(stderr, "snd_pcm_readi returned %ld (%s)\n", ret, ::snd_strerror(ret));

			::snd_pcm_recover(m_handle, ret, 1);
		}

		if (m_channels == 1U) {
			for (int n = 0; n < ret; n++)
				m_buffer[n] = float(m_samples[n]) / 32768.0F;
		} else {
			int i = 0;
			for (int n = 0; n < (ret * 2); n += 2)
				m_buffer[i++] = float(m_samples[n + 1]) / 32768.0F;
		}

		m_callback->readCallback(m_buffer, (unsigned int)ret);
	}

	::snd_pcm_close(m_handle);
}

void CSoundCardReader::kill()
{
	m_killed = true;
}

CSoundCardWriter::CSoundCardWriter(snd_pcm_t* handle, unsigned int blockSize, unsigned int channels, IAudioCallback* callback) :
CThread(),
m_handle(handle),
m_blockSize(blockSize),
m_channels(channels),
m_callback(callback),
m_killed(false),
m_buffer(NULL),
m_samples(NULL)
{
	assert(handle != NULL);
	assert(blockSize > 0U);
	assert(channels == 1U || channels == 2U);
	assert(callback != NULL);

	m_buffer  = new float[2U * blockSize];
	m_samples = new short[4U * blockSize];
}

CSoundCardWriter::~CSoundCardWriter()
{
	delete[] m_buffer;
	delete[] m_samples;
}

void CSoundCardWriter::entry()
{
	while (!m_killed) {
		int nSamples = 2U * m_blockSize;
		m_callback->writeCallback(m_buffer, nSamples);

		if (nSamples == 0U) {
			sleep(5UL);
		} else {
			if (m_channels == 1U) {
				for (int n = 0U; n < nSamples; n++)
					m_samples[n] = short(m_buffer[n] * 32767.0F);
			} else {
				int i = 0U;
				for (int n = 0U; n < nSamples; n++) {
					short sample = short(m_buffer[n] * 32767.0F);
					m_samples[i++] = sample;
					m_samples[i++] = sample;			// Same value to both channels
				}
			}

			int offset = 0U;
			snd_pcm_sframes_t ret;
			while ((ret = ::snd_pcm_writei(m_handle, m_samples + offset, nSamples - offset)) != (nSamples - offset)) {
				if (ret < 0) {
					if (ret != -EPIPE)
						::fprintf(stderr, "snd_pcm_writei returned %ld (%s)\n", ret, ::snd_strerror(ret));

					::snd_pcm_recover(m_handle, ret, 1);
				} else {
					offset += ret;
				}
			}
		}
	}

	::snd_pcm_close(m_handle);
}

void CSoundCardWriter::kill()
{
	m_killed = true;
}

bool CSoundCardWriter::isBusy() const
{
	snd_pcm_state_t state = ::snd_pcm_state(m_handle);

	return state == SND_PCM_STATE_RUNNING || state == SND_PCM_STATE_DRAINING;
}
