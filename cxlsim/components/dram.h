#include <map>
#include <fstream>
#include <thread>

#include <components/configurations.h>
#include <utilities/mm.h>
#include <utilities/statistics.h>
#include "tlm_utils/simple_target_socket.h"
#include "tlm_utils/peq_with_cb_and_phase.h"

#include "Bridge.h"

using namespace tlm;
using namespace sc_core;
using namespace ramulator2;

class Dram: public sc_module
{
public:
    SC_HAS_PROCESS(Dram);

    Dram(sc_module_name name, string config_name, int id);
    ~Dram();

	tlm_utils::simple_target_socket<Dram> slave;
    
	sc_in<bool> clock;

private:
	int id;    

	/* Backing store for memory. */
    uint8_t *mem_data;

	/* Incoming queues per channel. */
    deque<tlm_generic_payload *> w_queue;
    deque<tlm_generic_payload *> r_queue;	

	/* Payloads before processing */
	deque<tlm_generic_payload *>  mem_request;

	/* Completed payloads from DRAM */
	deque<tlm_generic_payload *> rack_queue;
	deque<tlm_generic_payload *> wack_queue;

	/* Key: payload, Value: time */
	map<tlm_generic_payload *, uint32_t> w_trans_map;
	map<tlm_generic_payload *, uint32_t> r_trans_map;

	void clock_posedge();
    void clock_negedge();

	tlm_sync_enum nb_transport_fw(tlm_generic_payload& trans, tlm_phase& phase, sc_time& t);

	void init_memdata();	
	void simulate_dram();
    void respond_read_request(tlm_generic_payload *outgoing);
    void respond_write_request(tlm_generic_payload *outgoing);
	void respond_write_request_together();
    void send_mem_request(tlm_generic_payload& payload);
	void update_payload_delay(tlm_generic_payload *payload, bool is_read);
	
	tlm_generic_payload * get_outgoing_payload(bool is_read);
	tlm_generic_payload* gen_trans(uint64_t addr, tlm_command cmd, uint32_t size, uint32_t id);
	
	void peq_cb(tlm_generic_payload& trans, const tlm_phase& phase);	
	void flit_packing(bool is_read);
	
	uint64_t active_cycle;
	uint64_t total_cycle;
	uint32_t m_outstanding;

	int slots_per_flit;
	int flit_stack;
	int ic_latency;

	Statistics *stats;	
    Bridge *bridge;
    mm m_mm;
	tlm_utils::peq_with_cb_and_phase<Dram> m_peq;
};
