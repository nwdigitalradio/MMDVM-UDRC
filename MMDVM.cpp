/*
 *   Copyright (C) 2015,2016,2017,2018 by Jonathan Naylor G4KLX
 *   Copyright (C) 2016 by Mathis Schmieder DB9MAT
 *   Copyright (C) 2016 by Colin Durbridge G4EML
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
#include "Globals.h"
#include "Thread.h"

#include <sys/types.h>
#include <pwd.h>
#include <unistd.h>
#include <getopt.h>
#include <systemd/sd-daemon.h>

// Global variables
MMDVM_STATE m_modemState = STATE_IDLE;

bool m_dstarEnable  = true;
bool m_dmrEnable    = true;
bool m_ysfEnable    = true;
bool m_p25Enable    = true;
bool m_nxdnEnable   = true;
bool m_pocsagEnable = true;

bool m_duplex = true;

bool m_tx  = false;
bool m_dcd = false;

CDStarRX   dstarRX;
CDStarTX   dstarTX;

CDMRDMORX  dmrDMORX;
CDMRDMOTX  dmrDMOTX;

CYSFRX     ysfRX;
CYSFTX     ysfTX;

CP25RX     p25RX;
CP25TX     p25TX;

CNXDNRX    nxdnRX;
CNXDNTX    nxdnTX;

CPOCSAGTX  pocsagTX;

CCalDStarRX calDStarRX;
CCalDStarTX calDStarTX;
CCalDMR     calDMR;
CCalP25     calP25;
CCalNXDN    calNXDN;

CCWIdTX cwIdTX;

CSerialPort serial;
CIO io;

void loop()
{

  serial.process();
//  io.process();

  // The following is for transmitting
  if (m_dstarEnable && m_modemState == STATE_DSTAR)
    dstarTX.process();

  if (m_dmrEnable && m_modemState == STATE_DMR)
    dmrDMOTX.process();

  if (m_ysfEnable && m_modemState == STATE_YSF)
    ysfTX.process();

  if (m_p25Enable && m_modemState == STATE_P25)
    p25TX.process();

  if (m_nxdnEnable && m_modemState == STATE_NXDN)
    nxdnTX.process();

  if (m_pocsagEnable && m_modemState == STATE_POCSAG)
    pocsagTX.process();

  if (m_modemState == STATE_DSTARCAL)
    calDStarTX.process();

  if (m_modemState == STATE_DMRCAL || m_modemState == STATE_LFCAL || m_modemState == STATE_DMRDMO1K)
    calDMR.process();

  if (m_modemState == STATE_P25CAL1K)
    calP25.process();

  if (m_modemState == STATE_NXDNCAL1K)
    calNXDN.process();

  if (m_modemState == STATE_IDLE)
    cwIdTX.process();
}

int main(int argc, char** argv)
{
    std::string ptyPath("/dev/ttyMMDVM0");

    int c;

    while (1) {
        static struct option long_options[] = {
            {"port",  required_argument, 0, 'p'},
            {"help",  no_argument,       0, 'h'},
            {0, 0, 0, 0}
        };

        int option_index = 0;

        c = getopt_long (argc, argv, "p:h", long_options, &option_index);

        if (c == -1)
            break;

        switch (c) {
            case 'p':
                ptyPath = optarg;
                break;

            case 'h':
            case '?':
                ::fprintf(stderr, "Usage: mmdvm-udrc [--port <vty port>]\n");
                exit(1);
                break;

            default:
                abort ();
        }
    }

    ::fprintf(stderr, "Opening PTY at %s\n", ptyPath.c_str());
    serial.setPtyPath(ptyPath);

    bool ret = serial.open();
    if (!ret) {
        ::fprintf(stderr,"Unable to open serial port on vpty: %s\n",ptyPath.c_str());
        exit(1);
    }

    CSoundCardReaderWriter sound("draws-capture-right", "draws-playback-right", 48000, 512);
    sound.setCallback(&io);

    ret = sound.open();
    if (!ret) {
        ::fprintf(stderr, "Unable to open audio device\n");
        return 1;
    }

    sd_notify(0, "READY=1");

    for (;;) {
        loop();
    }

    return 0;
}

