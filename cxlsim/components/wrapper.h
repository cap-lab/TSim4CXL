#include <deque>
#include <map>
#include <tuple>
#include <cmath> 

#include "tlm_utils/multi_passthrough_initiator_socket.h"
#include "tlm_utils/peq_with_cb_and_phase.h"

#include <components/host.h>
#include <components/configurations.h>
#include <utilities/mm.h>
#include <utilities/statistics.h>
#include <utilities/sync_object.h>

using namespace std;
using namespace tlm;
using namespace sc_core;

class Wrapper: public sc_module {

enum ChannelState
{
	CLEAR,
	REQ,
	ACK
};
		
public:
    SC_HAS_PROCESS(Wrapper);

   	sc_in<bool> clock;

    /* Outgoing channels */
    deque<tlm_generic_payload*> w_queue;
    deque<tlm_generic_payload*> r_queue;
    deque<tlm_generic_payload*> wack_queue;
    deque<tlm_generic_payload*> rack_queue;
    
	/* Synchronization */
	deque<int> sync_queue;
	deque<int> signal_queue;
	
	/*incoming communications to push at posedge*/
	deque<tlm_generic_payload*> total_pending;
    deque<tlm_generic_payload*> wack_incoming;
    deque<tlm_generic_payload*> rack_incoming;
	
	tlm_utils::multi_passthrough_initiator_socket<Wrapper> master;

    Wrapper(sc_module_name _name, int id, int num);
    ~Wrapper();

    void clock_posedge();
    void clock_negedge();
    void periodic_process();
	void flit_packing(bool is_read);
	void send_read_request(tlm_generic_payload *outgoing);
	void send_write_request(tlm_generic_payload *outgoing);
    void response_request(Packet *packet);
	tlm_sync_enum nb_transport_bw(int id, tlm_generic_payload& trans, tlm_phase& phase, sc_time& t);

private:
    void handle_packet(Packet *packet);
    void handle_read_packet(Packet *packet);
    void handle_write_packet(Packet *packet);
	void handle_signal_packet(Packet *packet);
	void handle_wait_packet(Packet *packet);
	void add_payload(tlm_command cmd, uint32_t address, uint32_t size, uint8_t* data, uint32_t device_id);
    void update_status(Status _status);
	
	/* Payload event queue callback to handle transactions from the target */
	void peq_cb(tlm_generic_payload& trans, const tlm_phase& phase);
	
	Statistics *stats;	
	
	/* Host */
    Host *host;
	int host_id;
	int rack_num;
	int wack_num;
	int w_idx;
	int r_idx;
	uint8_t* packet_data;
	
	/* Simulation settings */
	int cpu_latency;
	bool req_done;
	double period;
	bool received;
	uint32_t outstanding;	
	uint64_t active_cycle;
	uint64_t total_cycle;
	uint32_t r_req_size;
	uint32_t w_req_size;
	bool terminate;

	/* Flit packing related variables */
	int slots_per_flit;
	int flit_stack;
	int flit_size;
    tlm_generic_payload *waiting = NULL;
	mm m_mm;
 	tlm_utils::peq_with_cb_and_phase<Wrapper> m_peq;
};
