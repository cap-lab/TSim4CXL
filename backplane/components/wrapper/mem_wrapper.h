#include <map>
#include <fstream>
#include <thread>
#include <utilities/mm.h>
#include <utilities/statistics.h>
#include <utilities/configurations.h>
#include "tlm_utils/simple_target_socket.h"
#include "tlm_utils/peq_with_cb_and_phase.h"

#include "Bridge.h"

using namespace tlm;
using namespace sc_core;
using namespace ramulator2;

class MEMWrapper: public sc_module
{
public:
    SC_HAS_PROCESS(MEMWrapper);

	sc_in<bool> clock;
	
	/* Channels */
    deque<tlm_generic_payload *> w_queue;
    deque<tlm_generic_payload *> r_queue;	
	deque<tlm_generic_payload *> rack_queue;
	deque<tlm_generic_payload *> wack_queue;

	/* Payloads before processing */
	deque<tlm_generic_payload *>  mem_request;

	tlm_utils::simple_target_socket<MEMWrapper> slave;

    MEMWrapper(sc_module_name name, string config_name, int id);
    ~MEMWrapper();

	void simulate_dram();
	void clock_posedge();
    void clock_negedge();
	tlm_sync_enum nb_transport_fw(tlm_generic_payload& trans, tlm_phase& phase, sc_time& t);
    

private:
	void init();
	void init_memdata();	
    void backward_trans(tlm_generic_payload* trans, bool read);
	void update_trans_delay(uint32_t addr, bool is_read);
	void peq_cb(tlm_generic_payload& trans, const tlm_phase& phase);	
	tlm_generic_payload* gen_trans(uint64_t addr, tlm_command cmd, uint32_t size, uint32_t id);

	int id;    
    uint8_t *mem_data;
	uint64_t active_cycle;
	uint64_t total_cycle;
	uint32_t m_outstanding;
    mm m_mm;
	string name;
	sc_time t;

	map<uint32_t, uint32_t> w_trans_map;
	map<uint32_t, uint32_t> r_trans_map;

	Statistics *stats;	
    Bridge *bridge;
	tlm_utils::peq_with_cb_and_phase<MEMWrapper> m_peq;
};
