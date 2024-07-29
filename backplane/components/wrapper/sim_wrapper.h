#include <deque>
#include <map>
#include <tuple>
#include <cmath> 

#include "tlm_utils/multi_passthrough_initiator_socket.h"
#include "tlm_utils/peq_with_cb_and_phase.h"

#include <components/host.h>
#include <utilities/mm.h>
#include <utilities/statistics.h>
#include <utilities/sync_object.h>
#include <utilities/configurations.h>

using namespace std;
using namespace tlm;
using namespace sc_core;

class SIMWrapper: public sc_module {
public:
    SC_HAS_PROCESS(SIMWrapper);

   	sc_in<bool> clock;

    /* Channels */
    deque<tlm_generic_payload*> w_queue;
    deque<tlm_generic_payload*> r_queue;
    deque<tlm_generic_payload*> wack_queue;
    deque<tlm_generic_payload*> rack_queue;
    
	/* Synchronization */
	deque<uint32_t> sync_queue;
	
	/* Incoming payloads to push at posedge */
    tlm_generic_payload* wack_incoming;
    tlm_generic_payload* rack_incoming;
	
	tlm_utils::multi_passthrough_initiator_socket<SIMWrapper> master;

    SIMWrapper(sc_module_name _name, int id, int num);
    ~SIMWrapper();

    void clock_posedge();
    void clock_negedge();
    void periodic_process();
	tlm_sync_enum nb_transport_bw(int id, tlm_generic_payload& trans, tlm_phase& phase, sc_time& t);

private:
	void init();
    void response_request(Packet *packet);
    void handle_packet(Packet *packet);
    void handle_read_packet(Packet *packet);
    void handle_write_packet(Packet *packet);
	void handle_wait_packet(uint32_t addr);
    void update_status(Status _status);
	void forward_trans(tlm_generic_payload *trans);
	void add_payload(tlm_command cmd, uint32_t addr, uint32_t size, uint8_t* data, uint32_t burst_size, uint32_t device_id);
	void peq_cb(tlm_generic_payload& trans, const tlm_phase& phase);
	
	int host_id;
	bool req_done;
	double period;
	uint8_t* packet_data;
	uint32_t rack_num;
	uint32_t wack_num;
	uint32_t w_idx;
	uint32_t r_idx;
	uint32_t packet_size;
	uint32_t dram_req_size;
	uint32_t cpu_latency;
	uint32_t outstanding;	
	uint64_t active_cycle;
	uint64_t total_cycle;
	mm m_mm;
	string name;
	sc_time t;
	sc_time read_start;
	bool read_first = true;;

    Host *host;
	Statistics *stats;	
 	tlm_utils::peq_with_cb_and_phase<SIMWrapper> m_peq;
};
