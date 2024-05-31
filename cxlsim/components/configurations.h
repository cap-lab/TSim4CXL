#ifndef SIM_CONFIGURATIONS_H
#define SIM_CONFIGURATIONS_H

#include <iostream>
#include <stdexcept>
#include <fstream>
#include <stdlib.h>
#include <map>
#include <ext/jsoncpp/json.h>
#include <cmath>
using namespace std;

typedef struct _DRAMInfo {
    bool enabled;
    uint64_t size;
    string type;
    double freq;
	uint64_t width;
	uint64_t channel;
} DRAMInfo;

typedef struct _CPUInfo {
   	double simul_freq;
    double period;
	int cpu_latency;
} CPUInfo;

class Configurations
{
public:
    Configurations();

    void init_dram();
    void init_configurations();

    int get_link_latency() {return link_latency;}
    int get_host_num() { return host_num; }
    int get_dram_num() { return dram_num; }
    int get_flit_size() { return flit_size; }
    int get_packet_size() { return packet_size; }
	int get_dsp_per_usp() { return dsp_per_usp; }
    int get_port_latency() { return port_latency; };
    int get_cxl_ic_latency() { return ic_latency; };
    uint32_t get_dram_size(int id);
    double get_period(int id ); 
    double get_freq(int id); 
    double get_dram_freq(int id);
    int get_cpu_latency(int id);
    string get_dram_config(int id);

    bool dram_enabled();

private:
	int flit_size;
	int packet_size;
	int host_latency;
    double link_efficiency; 
    double raw_bandwidth;
    double link_bandwidth;
    double dram_bandwidth;
    int port_latency;
    int ic_latency;
    int link_latency; 
	int dsp_per_usp;
    int host_num;
    int dram_num;
	int dsp_id;
	int device_id;
    string output_dir;

    map<string, int> dram_list;
    map<int, DRAMInfo> dram_map;
    map<int, CPUInfo> cpu_map;
};

#endif
