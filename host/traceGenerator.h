#ifndef TRACE_GENERATOR_H
#define TRACE_GENERATOR_H
#pragma once
#include "packetManager.h"

class traceGenerator {
public:
    traceGenerator(int coreID, std::string name);
    
    void trcRead(uint32_t address, uint8_t* data, uint32_t size, uint32_t device, uint64_t delta);
    void trcWrite(uint32_t address, uint8_t* data, uint32_t size, uint32_t device, uint64_t delta);    
    void trcSignal(uint32_t signalID);
    void trcWait(uint32_t waitID);
    void trcTerminate(uint64_t delta);
	uint32_t get_packet_size();

private:
    int coreID;
    packetManager* manager;
};

#endif // TRACE_GENERATOR_H
