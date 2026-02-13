# Makefile

CXX = g++
CC = cc
CXXFLAGS = -std=c++11 -Wall
CFLAGS = -O2

TARGET = fxt65

OBJS = emu_sdmon.o vrEmu6502.o FxtSystem.o

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)

emu_sdmon.o: ./src/emu_sdmon.cpp
	$(CXX) $(CXXFLAGS) -c ./src/emu_sdmon.cpp

FxtSystem.o: src/FxtSystem.cpp src/FxtSystem.hpp
	$(CXX) $(CXXFLAGS) -c src/FxtSystem.cpp

vrEmu6502.o: ./src/lib/vrEmu6502.c
	$(CC) $(CFLAGS) -c ./src/lib/vrEmu6502.c

clean:
	rm -f *.o $(TARGET)

