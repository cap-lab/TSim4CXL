#include <map>
#include <components/configurations.h>

#include "tlm_utils/multi_passthrough_initiator_socket.h"
#include "tlm_utils/multi_passthrough_target_socket.h"

using namespace std;
using namespace tlm;
using namespace sc_core;

class Interconnector: public sc_module
{
public:
    SC_HAS_PROCESS(Interconnector);

    Interconnector(sc_module_name name);
    ~Interconnector();

	tlm_utils::multi_passthrough_initiator_socket<Interconnector> master;
	tlm_utils::multi_passthrough_target_socket<Interconnector> slave;

private:
	/* Key: payload address, Value: nb_transport ID */
	map<uint64_t, int> w_trans_id;	
	map<uint64_t, int> r_trans_id;	

	tlm_sync_enum nb_transport_fw(int id, tlm_generic_payload& trans, tlm_phase& phase, sc_time& t);
	tlm_sync_enum nb_transport_bw(int id, tlm_generic_payload& trans, tlm_phase& phase, sc_time& t);
};

