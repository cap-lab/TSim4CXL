#include <map>
#include <tuple>
#include <string>
#include <components/configurations.h>

#include "tlm_utils/multi_passthrough_initiator_socket.h"
#include "tlm_utils/multi_passthrough_target_socket.h"

using namespace std;
using namespace tlm;
using namespace sc_core;

class Port: public sc_module
{
public:
    SC_HAS_PROCESS(Port);

    Port(sc_module_name name, int id);
    ~Port();

	tlm_utils::multi_passthrough_initiator_socket<Port> master;
	tlm_utils::multi_passthrough_target_socket<Port> slave;
	void delayed_transport();

private:
	int id;
	int count_fw = 0;
	int count_bw = 0;
	int port_latency;
	int link_latency;
	uint32_t total_cycle;
	string port_name;
	double period;

	/* Key: payload address, Value: nb_transport ID */
	map<uint64_t, int> w_trans_id;	
	map<uint64_t, int> r_trans_id;	
	
	tlm_sync_enum nb_transport_fw(int id, tlm_generic_payload& trans, tlm_phase& phase, sc_time& t);
	tlm_sync_enum nb_transport_bw(int id, tlm_generic_payload& trans, tlm_phase& phase, sc_time& t);
	
	deque<tlm_generic_payload*> fw_queue;
	deque<tlm_generic_payload*> bw_queue;
};
