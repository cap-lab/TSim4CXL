CC = g++
CFLAGS = -std=c++11 -Wall -I ../backplane/utilities
VPATH = ../backplane/utilities

SOURCES = traceGenerator.cpp packetManager.cpp main.cpp packet_buffer.cpp
EXECUTABLE = host_exe 

$(EXECUTABLE): $(SOURCES)
	$(CC) $(CFLAGS) $^ -o $@ -lrt

.PHONY: clean

clean:
	rm -f $(EXECUTABLE)
