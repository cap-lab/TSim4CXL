#include "interconnector.h"

extern Configurations cfgs;
extern map<tlm_generic_payload *, uint32_t> device_map;

Interconnector::Interconnector(sc_module_name name) : sc_module(name), master("master"), slave("slave") 
{
	init();
	SC_THREAD(fw_thread);
	master.register_nb_transport_bw(this, &Interconnector::nb_transport_bw);
	slave.register_nb_transport_fw(this, &Interconnector::nb_transport_fw);
}

Interconnector::~Interconnector() {}

void Interconnector::init() {
	period = cfgs.get_period(0);
	link_latency = cfgs.get_link_latency();
	port_latency = cfgs.get_port_latency();
	dev_ic_latency = cfgs.get_cxl_dev_ic_latency();
	ctrl_latency = port_latency + dev_ic_latency;
}

void Interconnector::fw_thread() {
	tlm_phase phase = BEGIN_REQ;
	sc_time t = SC_ZERO_TIME;

	while(1) {
		/* READ */
		if (!r_queue.empty()) {
			if (count_fw%8 == 0)
				wait(ctrl_latency, SC_NS);
			else
				wait(link_latency, SC_NS);
			count_fw++;

			tlm_generic_payload *trans = r_queue.front();
			r_queue.pop_front();
			int id = device_map[trans];
			tlm_sync_enum reply = master[id]->nb_transport_fw(*trans, phase, t);
		}
		
		/* WRITE */
		if (!w_queue.empty()) {
			//wait(ctrl_latency, SC_NS);
			
			tlm_generic_payload *trans = w_queue.front();
			w_queue.pop_front();
			int id = device_map[trans];

			tlm_sync_enum reply = master[id]->nb_transport_fw(*trans, phase, t);
		}
		wait(period, SC_NS);
	}
}

tlm_sync_enum Interconnector::nb_transport_fw(int id, tlm_generic_payload& trans, tlm_phase& phase, sc_time& t) {
	if (phase == END_RESP)
		return TLM_COMPLETED;

	/* READ */
	if (trans.get_command() == TLM_READ_COMMAND) {
		r_trans_id[trans.get_address()] = id;
		r_queue.push_back(&trans);
	}

	/* WRITE */
	else {
		w_trans_id[trans.get_address()] = id;
		w_queue.push_back(&trans);
	}
	
	return TLM_UPDATED;
}

tlm_sync_enum Interconnector::nb_transport_bw(int id, tlm_generic_payload& trans, tlm_phase& phase, sc_time& t) {
	/* READ */
	if (trans.get_command() == TLM_READ_COMMAND) {
		id = r_trans_id[trans.get_address()];
	}

	/* WRITE */
	else {
		id = w_trans_id[trans.get_address()];
	}
	tlm_sync_enum reply = slave[id]->nb_transport_bw(trans, phase, t);
	return reply;
}
