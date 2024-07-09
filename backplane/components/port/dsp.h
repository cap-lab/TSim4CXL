#include <string>
#include <utilities/configurations.h>
#include <utilities/statistics.h>
#include "tlm_utils/multi_passthrough_initiator_socket.h"
#include "tlm_utils/multi_passthrough_target_socket.h"

using namespace std;
using namespace tlm;
using namespace sc_core;

/* Downstream Port */
class DSP: public sc_module
{
public:
    SC_HAS_PROCESS(DSP);
	sc_in<bool> clock;

    DSP(sc_module_name name, int id);
    ~DSP();

	tlm_utils::multi_passthrough_initiator_socket<DSP> master;
	tlm_utils::multi_passthrough_target_socket<DSP> slave;
    void flit_packing_68();
    void flit_packing_256();
    void fw_thread();
    void bw_thread();
    void init();

private:
	uint32_t id;
	uint32_t w_msg;
	uint32_t req_num;
	uint32_t flit_mode;
	uint32_t port_latency;
	uint32_t link_latency;
	uint32_t flit_num;
	uint32_t fw_cnt; 
	double period;
	string name;
	sc_time t;

	tlm_sync_enum nb_transport_fw(int id, tlm_generic_payload& trans, tlm_phase& phase, sc_time& t);
	tlm_sync_enum nb_transport_bw(int id, tlm_generic_payload& trans, tlm_phase& phase, sc_time& t);

	deque<tlm_generic_payload*> r_queue;
	deque<tlm_generic_payload*> w_queue;
	deque<tlm_generic_payload*> rack_queue;
	deque<tlm_generic_payload*> wack_queue;
	deque<tlm_generic_payload*> pending_queue;

	Statistics *stats;	
};
