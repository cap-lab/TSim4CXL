CC = g++
CFLAGS = -std=c++11 -Wall -I ../backplane/utilities
VPATH = ../backplane/utilities

SOURCES = traceGenerator.cpp packetManager.cpp main.cpp packet_buffer.cpp
OBJECTS = $(SOURCES:.cpp=.o)
EXECUTABLE = host_exe 

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(CFLAGS) $^ -o $@ -lrt

%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean

clean:
	rm -f $(OBJECTS) $(EXECUTABLE)
