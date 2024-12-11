#include <map>
#include <utilities/configurations.h>
#include <utilities/statistics.h>

#include "tlm_utils/multi_passthrough_initiator_socket.h"
#include "tlm_utils/multi_passthrough_target_socket.h"

using namespace std;
using namespace tlm;
using namespace sc_core;

class Controller: public sc_module
{
public:
    SC_HAS_PROCESS(Controller);

    Controller(sc_module_name name);
    ~Controller();

	tlm_utils::multi_passthrough_initiator_socket<Controller> master;
	tlm_utils::multi_passthrough_target_socket<Controller> slave;

private:
	/* Key: payload address, Value: nb_transport ID */
	map<uint64_t, int> w_trans_id;	
	map<uint64_t, int> r_trans_id;	

	tlm_sync_enum nb_transport_fw(int id, tlm_generic_payload& trans, tlm_phase& phase, sc_time& t);
	tlm_sync_enum nb_transport_bw(int id, tlm_generic_payload& trans, tlm_phase& phase, sc_time& t);
	
	void init();
	void fw_thread();
	void bw_thread();
    void flit_packing_68();

	double period;
	uint32_t id;
	uint32_t r_num;
	uint32_t w_num;
	uint32_t pipeline;
	uint32_t link_latency;
	uint32_t ctrl_latency;

	string name;
	sc_time t;

	deque<tlm_generic_payload*> r_queue;
	deque<tlm_generic_payload*> w_queue;
	deque<tlm_generic_payload*> rack_queue;
	deque<tlm_generic_payload*> wack_queue;
	
	Statistics *stats;	
};

