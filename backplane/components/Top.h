#include <iostream>
#include <iomanip>
#include "systemc.h"
#include "tlm.h"

#include <components/wrapper/sim_wrapper.h>
#include <components/wrapper/mem_wrapper.h>
#include <components/interconnector.h>
#include <components/port/usp.h>
#include <components/port/dsp.h>
#include <utilities/sync_object.h>

using namespace sc_core;
using namespace std;

extern Configurations cfgs;
SyncObject sync_object;

uint32_t active_cores = 0;
uint32_t active_dram = 0;

SC_MODULE(Top)
{
    SIMWrapper** sim_wrapper;
    MEMWrapper** mem_wrapper;
	USP** upstream_port;
	DSP** downstream_port;
	Interconnector* interconnector;
	sc_in<bool>* clock;

    SC_CTOR(Top)
    {
		host_num = cfgs.get_host_num();
		dram_num = cfgs.get_dram_num();

		/* Instantiate components */
		sim_wrapper = new SIMWrapper*[host_num];
    	mem_wrapper = new MEMWrapper*[dram_num];
    	upstream_port = new USP*[host_num];
    	downstream_port = new DSP*[host_num];
		clock = new sc_in<bool>[host_num];
		interconnector = new Interconnector("ic");


		for (int i = 0; i < host_num; i++) {
			string wrapper_name = "host_" + to_string(i);
			active_cores++;	 
			sim_wrapper[i] = new SIMWrapper(wrapper_name.c_str(), i, host_num);
			sim_wrapper[i]->clock.bind(clock[i]);
			
			/* USP&DSP */
			string usp_name = "host_" + to_string(i) + "_usp_" + to_string(i);
			string dsp_name = "host_" + to_string(i) + "_dsp_" + to_string(i);
			upstream_port[i] = new USP(usp_name.c_str(), i);
			upstream_port[i]->clock.bind(clock[i]);
			downstream_port[i] = new DSP(dsp_name.c_str(), i);
			downstream_port[i]->clock.bind(clock[i]);
			
			/* Bind the modules */
			sim_wrapper[i]->master.bind(upstream_port[i]->slave);
			upstream_port[i]->master.bind(downstream_port[i]->slave);
			downstream_port[i]->master.bind(interconnector->slave);
		}

		// Memory
		for (int i = 0; i < dram_num; i++) {
			string module_name = "mem_" + to_string(i);
			stringstream config;
			config << cfgs.get_dram_config(i);
			mem_wrapper[i] = new MEMWrapper(module_name.c_str(), config.str(), i);
			mem_wrapper[i]->clock.bind(clock[i%host_num]);
			
			interconnector->master.bind(mem_wrapper[i]->slave);
		}
		
		active_dram |= 1;
    }

    ~Top() {
    }

    void finish() {
		if (sim_wrapper) {
			for (int i = 0; i < host_num; i++) {
				if (sim_wrapper[i]) {
					delete (sim_wrapper[i]);
					sim_wrapper[i] = NULL;
				}
			}
		}
		if (upstream_port) {
			for (int i = 0; i < host_num; i++) {
				if (upstream_port[i]) {
					delete upstream_port[i];
					upstream_port[i] = NULL;
				}
			}
		}
		if (downstream_port) {
			for (int i = 0; i < host_num; i++) {
				if (downstream_port[i]) {
					delete downstream_port[i];
					downstream_port[i] = NULL;
				}
			}
		}
		if (interconnector) {
			delete interconnector;
			interconnector = NULL;
		}
		if (mem_wrapper) {
			for (int i = 0; i < dram_num; i++) {
				if (mem_wrapper[i]) {
					delete (mem_wrapper[i]);
					mem_wrapper[i] = NULL;
				}
			}
		}
    }

    int host_num;
	int dram_num;
};
