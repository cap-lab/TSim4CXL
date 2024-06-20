#ifndef SIM_CONFIGURATIONS_H
#define SIM_CONFIGURATIONS_H

#include <iostream>
#include <stdexcept>
#include <fstream>
#include <stdlib.h>
#include <map>
#include <json.h>
#include <cmath>
using namespace std;

typedef struct _DRAMInfo {
    string type;
    uint64_t size;
    double freq;
} DRAMInfo;

typedef struct _CPUInfo {
   	double simul_freq;
    double period;
	uint32_t cpu_latency;
} CPUInfo;

class Configurations
{
public:
    Configurations();

    void init_dram();
    void init_configurations();
    uint32_t get_link_latency() {return link_latency;}
    uint32_t get_host_num() { return host_num; }
    uint32_t get_dram_num() { return dram_num; }
    uint32_t get_flit_mode() { return flit_mode; }
    uint32_t get_packet_size() { return packet_size; }
    uint32_t get_dram_req_size() { return dram_req_size; };
    uint32_t get_port_latency() { return port_latency; };
    uint32_t get_cxl_ic_latency() { return ic_latency; };
    uint32_t get_cpu_latency(int id);
    uint64_t get_dram_size(int id);
    double get_period(int id ); 
    double get_freq(int id); 
    double get_dram_freq(int id);
    string get_dram_config(int id);
    bool dram_enabled();

private:
	uint32_t flit_mode;
	uint32_t flit_size;
	uint32_t packet_size;
	uint32_t dram_req_size;
	uint32_t host_latency;
    uint32_t port_latency;
    uint32_t ic_latency;
    uint32_t link_latency; 
    uint32_t host_num;
    uint32_t dram_num;
    double link_efficiency; 
    double raw_bandwidth;
    double link_bandwidth;
    string output_dir;

    map<string, int> dram_list;
    map<int, DRAMInfo> dram_map;
    map<int, CPUInfo> cpu_map;
};

#endif
