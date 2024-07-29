#include "dsp.h"

extern Configurations cfgs;

DSP::DSP(sc_module_name name, int id) : sc_module(name), master("master"), slave("slave"), clock("clock"),
		id(id), name(string(name)), t(SC_ZERO_TIME)
{
	init();
	SC_THREAD(fw_thread);
	SC_THREAD(bw_thread);
	master.register_nb_transport_bw(this, &DSP::nb_transport_bw);
	slave.register_nb_transport_fw(this, &DSP::nb_transport_fw);
}

DSP::~DSP() {}

void DSP::init() {
	period = cfgs.get_period(id);
}

void DSP::fw_thread() {
	tlm_phase phase = BEGIN_REQ;

	while(1) {
		/* READ */
		if (!r_queue.empty()) {
			tlm_generic_payload *trans = r_queue.front();
			r_queue.pop_front();
			tlm_sync_enum reply = master->nb_transport_fw(*trans, phase, t);
		}

		/* WRITE */
		if (!w_queue.empty()) {
			tlm_generic_payload *trans = w_queue.front();
			w_queue.pop_front();
			tlm_sync_enum reply = master->nb_transport_fw(*trans, phase, t);
		}	
		wait(period, SC_NS);
	}
}

void DSP::bw_thread() {
	tlm_phase phase = BEGIN_RESP;

	while(1) {
		/* READ */
		if (!rack_queue.empty()) {
			tlm_generic_payload *trans = rack_queue.front();
			rack_queue.pop_front();
			tlm_sync_enum reply = slave->nb_transport_bw(*trans, phase, t);
		}

		/* WRITE */
		if (!wack_queue.empty()) {
			tlm_generic_payload *trans = wack_queue.front();
			wack_queue.pop_front();
			tlm_sync_enum reply = slave->nb_transport_bw(*trans, phase, t);
		}	
		wait(period, SC_NS);
	}

}

tlm_sync_enum DSP::nb_transport_fw(int id, tlm_generic_payload& trans, tlm_phase& phase, sc_time& t) {
	/* READ */
	if (trans.get_command() == TLM_READ_COMMAND) {
		r_queue.push_back(&trans);
	}

	/* WRITE */
	else {
		w_queue.push_back(&trans);
	}

	return TLM_UPDATED;
}

tlm_sync_enum DSP::nb_transport_bw(int id, tlm_generic_payload& trans, tlm_phase& phase, sc_time& t) {
	/* READ */
	if (trans.get_command() == TLM_READ_COMMAND) {
		rack_queue.push_back(&trans);
	}

	/* WRITE */
	else {
		wack_queue.push_back(&trans);
	}

	return TLM_UPDATED;
}
