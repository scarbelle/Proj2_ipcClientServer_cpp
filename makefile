#
# CSE 691 Adv Systems Programming
# Homework #7
#
# Scarlett J. Davidson
#
# Makefile to build main.c
#
#

CXX=g++
CPPFLAGS = -g -lrt -pthread
LDFLAGS = -g -lrt -pthread

OBJS= ipcCommon.o ipcMain.o ipcCSProg.o ipcServer.o ipcSharedMem.o ipcFifo.o
TARGET=myproj2

all: $(TARGET)

ipcCommon.o: ipcCommon.h

ipcMain.o: ipcMain.cpp ipcCommon.h ipcServer.h


ipcCSProg.o: ipcCommon.h


ipcServer.o: ipcCommon.h ipcCSProg.h ipcSharedMem.h ipcFifo.h


ipcSharedMem.o: ipcSharedMem.h

ipcFifo.o: ipcCommon.h ipcFifo.h

myproj2: $(OBJS)
	echo NO EXECUTABLE YET

%.o:%cpp
	$(CXX) $(CPPFLAGS) -c $<






