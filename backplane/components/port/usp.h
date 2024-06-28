#include <string>
#include <utilities/statistics.h>
#include <utilities/configurations.h>
#include "tlm_utils/multi_passthrough_initiator_socket.h"
#include "tlm_utils/multi_passthrough_target_socket.h"

using namespace std;
using namespace tlm;
using namespace sc_core;

/* Upstream Port */
class USP: public sc_module
{
public:
    SC_HAS_PROCESS(USP);
	sc_in<bool> clock;

    USP(sc_module_name name, int id);
    ~USP();

	tlm_utils::multi_passthrough_initiator_socket<USP> master;
	tlm_utils::multi_passthrough_target_socket<USP> slave;
    void flit_packing_68(bool read);
    void flit_packing_256(bool read);
    void fw_thread();
    void bw_thread();
	void init();

private:
	uint32_t id;
	uint32_t flit_mode;
	uint32_t remainder;
	uint32_t port_latency;
	uint32_t link_latency;
	uint32_t f_idx;
	uint32_t r_msg;
	uint32_t w_msg;
	uint32_t last_flit;
	uint32_t w_flit_stack;
	uint32_t dram_req_size;
	uint32_t fw_cnt;
	uint32_t bw_cnt;
	double period;
	string name;

	tlm_sync_enum nb_transport_fw(int id, tlm_generic_payload& trans, tlm_phase& phase, sc_time& t);
	tlm_sync_enum nb_transport_bw(int id, tlm_generic_payload& trans, tlm_phase& phase, sc_time& t);
	
	deque<tlm_generic_payload*> r_queue;
	deque<tlm_generic_payload*> w_queue;
	deque<tlm_generic_payload*> rack_queue;
	deque<tlm_generic_payload*> wack_queue;
	deque<tlm_generic_payload*> pending_queue;

	Statistics *stats;	
};
