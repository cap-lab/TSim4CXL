#include <iostream>
#include <iomanip>
#include <vector>
#include "systemc.h"
#include "tlm.h"

#include <components/wrapper.h>
#include <components/dram.h>
#include <components/interconnector.h>
#include <components/port.h>
#include <utilities/sync_object.h>

using namespace sc_core;
using namespace std;

extern Configurations cfgs;
SyncObject sync_object;

uint32_t active_cores = 0;
uint32_t active_dram = 0;

SC_MODULE(Top)
{
    Wrapper** wrapper;
    Dram** dram;
	Port** upstream_port, **downstream_port;
	Interconnector* interconnector;
	sc_in<bool>* clock;

    SC_CTOR(Top)
    {
        host_num = cfgs.get_host_num();
        dram_num = cfgs.get_dram_num();
        
		// Instantiate components
        wrapper = new Wrapper*[host_num];
    	dram = new Dram*[dram_num];
    	upstream_port = new Port*[host_num*2];
    	downstream_port = new Port*[host_num*2];
		clock = new sc_in<bool>[host_num];
		interconnector = new Interconnector("ic");


		for (int i = 0; i < host_num; i++) {
			string wrapper_name = "host_" + to_string(i);
			active_cores++;	 
			wrapper[i] = new Wrapper(wrapper_name.c_str(), i, host_num);
			wrapper[i]->clock.bind(clock[i]);
			
			// Write/Read port	
			for (int j = 0; j < 2; j++) {	
				int id = i*2 + j;
				string usp_name = "host_" + to_string(i) + "_usp_" + to_string(id);
				string dsp_name = "host_" + to_string(i) + "_dsp_" + to_string(id);
				upstream_port[id] = new Port(usp_name.c_str(), id);
				downstream_port[id] = new Port(dsp_name.c_str(), id);
			
				// Bind the modules
				wrapper[i]->master.bind(upstream_port[id]->slave);
				upstream_port[id]->master.bind(downstream_port[id]->slave);
				downstream_port[id]->master.bind(interconnector->slave);
			}
		}

		// Memory
		for (int i = 0; i < dram_num; i++) {
			string module_name = "mem_" + to_string(i);
			stringstream config;
	        config << cfgs.get_dram_config(i);
	   		dram[i] = new Dram(module_name.c_str(), config.str(), i);
			dram[i]->clock.bind(clock[i%host_num]);
			
			interconnector->master.bind(dram[i]->slave);
		}
		
		active_dram |= 1;
    }

    ~Top() {
    }

    void finish() {
        if (wrapper) {
            for (int i = 0; i < host_num; i++) {
                if (wrapper[i]) {
                    delete (wrapper[i]);
                    wrapper[i] = NULL;
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
		if (dram) {
            for (int i = 0; i < dram_num; i++) {
                if (dram[i]) {
                    delete (dram[i]);
                    dram[i] = NULL;
                }
            }
		}
    }

    int host_num;
	int dram_num;
};
