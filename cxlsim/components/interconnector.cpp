#include "interconnector.h"

extern map<tlm_generic_payload *, uint32_t> device_map;

Interconnector::Interconnector(sc_module_name name) : sc_module(name), master("master"), slave("slave") 
{
	master.register_nb_transport_bw(this, &Interconnector::nb_transport_bw);
	slave.register_nb_transport_fw(this, &Interconnector::nb_transport_fw);
}

Interconnector::~Interconnector() {
}

tlm_sync_enum Interconnector::nb_transport_fw(int id, tlm_generic_payload& trans, tlm_phase& phase, sc_time& t) {
	if (phase == END_RESP)
		return TLM_COMPLETED;

	if (trans.get_command() == TLM_READ_COMMAND)
		r_trans_id[trans.get_address()] = id;
	else
		w_trans_id[trans.get_address()] = id;
	
	id = device_map[&trans];
	tlm_sync_enum reply = master[id]->nb_transport_fw(trans, phase, t);
	return reply;
}

tlm_sync_enum Interconnector::nb_transport_bw(int id, tlm_generic_payload& trans, tlm_phase& phase, sc_time& t) {
	if (trans.get_command() == TLM_READ_COMMAND)
		id = r_trans_id[trans.get_address()];
	else 
		id = w_trans_id[trans.get_address()];
	
	tlm_sync_enum reply = slave[id]->nb_transport_bw(trans, phase, t);
	return reply;
}
