CXX     = g++
CFLAGS  = -g -O3 -Wall -std=c++0x -pthread -ffast-math -mfpu=neon-vfpv4 -funsafe-math-optimizations
LIBS    = -lpthread -lasound -lwiringPi -lpulse -lsystemd
LDFLAGS = -g

OBJECTS = Biquad.o CalDMR.o CalDStarRX.o CalDStarTX.o CalNXDN.o CalP25.o CWIdTX.o DMRDMORX.o DMRDMOTX.o \
	  DMRSlotType.o DStarRX.o DStarTX.o FIR.o FIRInterpolator.o IO.o IOUDRC.o MMDVM.o NXDNRX.o \
	  NXDNTX.o P25RX.o P25TX.o POCSAGTX.o SampleRB.o SerialPort.o SerialRB.o \
	  SoundCardReaderWriter.o Thread.o Utils.o YSFRX.o YSFTX.o

all:	mmdvm-udrc

mmdvm-udrc:	$(OBJECTS)
	$(CXX) $(OBJECTS) $(LDFLAGS) $(LIBS) -o mmdvm-udrc

%.o: %.cpp
	$(CXX) $(CFLAGS) -c -o $@ $<

clean:
	$(RM) mmdvm-udrc *.o *.d *.bak *~

