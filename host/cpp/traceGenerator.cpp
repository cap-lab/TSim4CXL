#include "traceGenerator.h"
#include <regex>

traceGenerator::traceGenerator(int coreID, string name) : coreID(coreID) {
    manager = new traceManager(coreID, name);
	packet_size = get_packet_size();
}

uint32_t traceGenerator::get_packet_size() {
	ifstream ifs("../../cxlsim/configs/system.json");
	if (!ifs.is_open()) {
		cerr << "Failed to open system.json" << endl;
		return 1;
    }
	string json_contents((istreambuf_iterator<char>(ifs)),(istreambuf_iterator<char>()));
    regex regex("\"packet_size\"\\s*:\\s*(\\d+)");
    smatch match;
    if (regex_search(json_contents, match, regex)) {
		int packet_size = stoi(match[1]);
		return packet_size;
	}
	return 1;
}

void traceGenerator::trcWrite(uint32_t address, uint8_t* data, uint32_t size, uint32_t device, uint64_t cycle) {
	size_t j = 0;
    for (int i = size; i > 0; i -= packet_size) {
		if (j > 0) { cycle = 0; }
		manager->writeRequest(address+(j*packet_size), data+(j*packet_size), packet_size, device, cycle); 
		j++;
    }
}

void traceGenerator::trcRead(uint32_t address, uint8_t* data, uint32_t size, uint32_t device, uint64_t cycle) {
    size_t j = 0;
    for (int i = size; i > 0 ; i -= packet_size) {
		if (j > 0) { cycle = 0; }
		manager->readRequest(address+(j*packet_size), packet_size, device, cycle);
		uint8_t* read_data = new uint8_t[packet_size];
		manager->readPacketData(read_data);
		memcpy(data+(j*packet_size), read_data, packet_size);
		j++;
	}
}

void traceGenerator::trcSignal(uint32_t signalID) {
    manager->signalRequest(signalID);
}

void traceGenerator::trcWait(uint32_t waitID) {
    manager->waitRequest(waitID);
}

void traceGenerator::trcTerminate(uint64_t cycle) {
    manager->terminateRequest(cycle);
}
