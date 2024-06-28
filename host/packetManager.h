#ifndef TRACE_MANAGER_H
#define TRACE_MANAGER_H
#pragma once
#include <iostream>
#include <sys/mman.h>
#include <fcntl.h>
#include <sstream>
#include <fstream>
#include <pwd.h>
#include <unistd.h>
#include <cassert>
#include <string.h>
#include <tuple>
#include <cstdlib>
#include <vector>
#include <iterator>
#include <regex>
#include <cmath>
#include "packet_buffer.h"

using namespace std;

class packetManager {
public:
    packetManager(int coreID, string name);
    bool is_empty();
    bool prepare_connection(int coreID, string name);
    void readRequest(uint32_t address, uint32_t size, uint32_t device, uint64_t delta);
    void writeRequest(uint32_t address, uint8_t* data, uint32_t size, uint32_t device, uint64_t delta);
    void signalRequest(uint32_t signalID);
    void waitRequest(uint32_t signalID);
    void terminateRequest(uint64_t delta);
    void* establish_shm_segment(char *name, int size);
    void sendPacket(Packet * pPacket);
    void recvPacket(Packet* pPacket);
    void readPacketData(uint8_t *data);
    Packet* makePacket(PacketType type, uint32_t address, uint8_t* data, uint32_t size, uint32_t device, uint64_t delta);

private:
    int coreID;
	char *bi_name;
	char *ib_name;
    PacketBuffer *sendBuffer;
    PacketBuffer *recvBuffer;

    struct timespec ts;
};

#endif // TRACE_MANAGER_H
