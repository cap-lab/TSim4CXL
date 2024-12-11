#include "controller.h"

extern Configurations cfgs;
extern map<tlm_generic_payload *, uint32_t> device_map;

Controller::Controller(sc_module_name name) : sc_module(name), master("master"), slave("slave"),
	name(string(name)), w_num(0), r_num(0), pipeline(0), t(SC_ZERO_TIME)
{
	init();
	SC_THREAD(fw_thread);
	SC_THREAD(bw_thread);
	master.register_nb_transport_bw(this, &Controller::nb_transport_bw);
	slave.register_nb_transport_fw(this, &Controller::nb_transport_fw);
}

Controller::~Controller() {
	if (stats) {
		stats->print_stats();
		free(stats);
		stats = NULL;
	}
}


void Controller::init() {
	period = cfgs.get_period(0);
	link_latency = cfgs.get_link_latency();
	ctrl_latency = cfgs.get_cxl_ctrl_latency();
	
	stats = new Statistics();
	stats->set_name(name);

	pipeline = 16; /* Request pipeline cycle */
}

void Controller::fw_thread() {
	tlm_phase phase = BEGIN_REQ;
	sc_time t = SC_ZERO_TIME;

	while(1) {
		/* READ */
		if (!r_queue.empty()) {
			if (r_num%pipeline==0) {
				wait(ctrl_latency, SC_NS);
			} else {
				wait(link_latency, SC_NS);
			}
			
			tlm_generic_payload *trans = r_queue.front();
			r_queue.pop_front();
			int id = device_map[trans];

			tlm_sync_enum reply = master[id]->nb_transport_fw(*trans, phase, t);
			r_num++;
		}
		
		/* WRITE */
		if (!w_queue.empty()) {
			if (w_num%pipeline==0) {
				wait(ctrl_latency, SC_NS);
			} else {
				wait(link_latency, SC_NS);
			}

			tlm_generic_payload *trans = w_queue.front();
			w_queue.pop_front();
			int id = device_map[trans];

			tlm_sync_enum reply = master[id]->nb_transport_fw(*trans, phase, t);
			w_num++;
		}
		wait(period, SC_NS);
	}
}

void Controller::bw_thread() {
	while(1) {
		flit_packing_68();
		wait(period, SC_NS);
	}
}

void Controller::flit_packing_68() {
	tlm_phase phase = BEGIN_RESP;
	
	if (!rack_queue.empty() || !wack_queue.empty()) {
		/* Only DRS */
		if (wack_queue.empty()) {
			wait(ctrl_latency+link_latency, SC_NS);
	
			/* MAX 3DRS = 4flits */
			for (int i = 0; i < 3; i++) {
				if (rack_queue.empty())
					break;

				/* Flit transfer latency */
				wait(period, SC_NS);
				tlm_generic_payload *trans = rack_queue.front();
				rack_queue.pop_front();
				id = r_trans_id[trans->get_address()];

				tlm_sync_enum reply = slave[id]->nb_transport_bw(*trans, phase, t);
			}
		}

		/* Only NDR */
		else if (rack_queue.empty()) {
			wait(ctrl_latency+link_latency, SC_NS);
			
			/* MAX 2NDR = 1flit */
			for (int i = 0; i < 2; i++) {
				if (wack_queue.empty())
					break;
					
				wait(period, SC_NS);
				tlm_generic_payload *trans = wack_queue.front();
				wack_queue.pop_front();
 				id = w_trans_id[trans->get_address()];

				tlm_sync_enum reply = slave[id]->nb_transport_bw(*trans, phase, t);
			}
		}
		
		/* NDR+DRS */
		else {
			wait(ctrl_latency+link_latency, SC_NS);

			/* 2NDR+1DRS = 2flits */
			for (int i = 0; i < 2; i++) {
				if (wack_queue.empty())
					break;

				tlm_generic_payload *trans = wack_queue.front();
				wack_queue.pop_front();
 				id = w_trans_id[trans->get_address()];

				tlm_sync_enum reply = slave[id]->nb_transport_bw(*trans, phase, t);
			}

			wait(period, SC_NS);
			tlm_generic_payload *trans = rack_queue.front();
			rack_queue.pop_front();
			id = r_trans_id[trans->get_address()];
			tlm_sync_enum reply = slave[id]->nb_transport_bw(*trans, phase, t);
		}
	}
}

tlm_sync_enum Controller::nb_transport_fw(int id, tlm_generic_payload& trans, tlm_phase& phase, sc_time& t) {
    auto& queue = (trans.get_command() == TLM_READ_COMMAND) ? r_queue : w_queue;
    auto& trans_id = (trans.get_command() == TLM_READ_COMMAND) ? r_trans_id : w_trans_id;

    trans_id[trans.get_address()] = id;
    queue.push_back(&trans);
	
	return TLM_UPDATED;
}

tlm_sync_enum Controller::nb_transport_bw(int id, tlm_generic_payload& trans, tlm_phase& phase, sc_time& t) {
    auto& queue = (trans.get_command() == TLM_READ_COMMAND) ? rack_queue : wack_queue;
    auto& trans_id = (trans.get_command() == TLM_READ_COMMAND) ? r_trans_id : w_trans_id;

    id = trans_id[trans.get_address()];
    queue.push_back(&trans);
	
	return TLM_UPDATED;
}
