#include "controller.h"

extern Configurations cfgs;
extern map<tlm_generic_payload *, uint32_t> device_map;

Controller::Controller(sc_module_name name) : sc_module(name), master("master"), slave("slave"),
	name(string(name)), w_msg(0), r_msg(0), w_num(0), r_num(0), flit_num(0), t(SC_ZERO_TIME)
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
	flit_mode = cfgs.get_flit_mode();
	dram_num = cfgs.get_dram_num();
	link_latency = cfgs.get_link_latency();
	ctrl_latency = cfgs.get_cxl_ctrl_latency();
	dev_ic_latency = cfgs.get_cxl_dev_ic_latency();
	req_num = cfgs.get_packet_size()/cfgs.get_dram_req_size();
	stats = new Statistics();
	stats->set_name(name);

	/* Flit-Packing variables */	
	r_msg = (flit_mode == 68) ? 2 : 8;
	w_msg = (flit_mode == 68) ? 2 : 12;
}

void Controller::fw_thread() {
	tlm_phase phase = BEGIN_REQ;
	sc_time t = SC_ZERO_TIME;

	while(1) {
		/* READ */
		if (!r_queue.empty()) {

			if (r_num == r_msg) {
				wait(ctrl_latency, SC_NS);
			}

			else if (r_num%r_msg == 0) {
				wait(link_latency, SC_NS);
				if (r_num == req_num) {
					r_num = 0;
				}
			}

			tlm_generic_payload *trans = r_queue.front();
			r_queue.pop_front();
			int id = device_map[trans];
			tlm_sync_enum reply = master[id]->nb_transport_fw(*trans, phase, t);
		}
		
		/* WRITE */
		if (!w_queue.empty()) {
			tlm_generic_payload *trans = w_queue.front();
			w_queue.pop_front();
			int id = device_map[trans];

			if (w_num == 1) {
				wait(ctrl_latency, SC_NS);
			}
			else {
				wait(link_latency, SC_NS);
				if (w_num == req_num) {
					w_num = 0;
				}
			}

			tlm_sync_enum reply = master[id]->nb_transport_fw(*trans, phase, t);
		}
		wait(period, SC_NS);
	}
}

void Controller::bw_thread() {
	while(1) {
		flit_mode == 68 ? flit_packing_68() : flit_packing_256();
		wait(period, SC_NS);
	}
}

void Controller::flit_packing_68() {
	tlm_phase phase = BEGIN_RESP;
	
	/* Only W flit */
	if (wack_queue.size() >= w_msg) {
		/* CXL Latency (per flit) */
		wait(dev_ic_latency/dram_num, SC_NS);
		stats->increase_total_flit();

		for (int i = 0; i < w_msg; i++) {
			tlm_generic_payload *trans = wack_queue.front();
			wack_queue.pop_front();
 			id = w_trans_id[trans->get_address()];
			tlm_sync_enum reply = slave[id]->nb_transport_bw(*trans, phase, t);
		}
	}
	/* 4DRS per 5flits */
	else if (rack_queue.size() >= 4) {
		
		/* CXL Latency (5flits) */
		for (int i = 0; i < 5; i++) {
			stats->increase_total_flit();
		}

		/* W/R flit together (1NDR+4DRS per 5flits) */
		if (!wack_queue.empty()) {
			tlm_generic_payload *trans = wack_queue.front();
			wack_queue.pop_front();
 			id = w_trans_id[trans->get_address()];
			tlm_sync_enum reply = slave[id]->nb_transport_bw(*trans, phase, t);
		}
		
		/* 4DRS */	
		for (int i = 0; i < 4; i++) {
			wait(period, SC_NS);
			tlm_generic_payload *trans = rack_queue.front();
			rack_queue.pop_front();
 			id = r_trans_id[trans->get_address()];
	
			if (i == 0) {
				wait(ctrl_latency, SC_NS);
			}		
			else {
				wait(dev_ic_latency/dram_num, SC_NS);
			}

			tlm_sync_enum reply = slave[id]->nb_transport_bw(*trans, phase, t);
		}
	}
}

void Controller::flit_packing_256() {
	tlm_phase phase = BEGIN_RESP;

	/* W flit only */
	if (wack_queue.size() >= w_msg) {
		/* CXL Latency (per flit) */
		//wait(ctrl_latency, SC_NS);
		stats->increase_total_flit();
		
		for (int i = 0; i < w_msg; i++) {
			tlm_generic_payload *trans = wack_queue.front();
			wack_queue.pop_front();
 			id = w_trans_id[trans->get_address()];
			tlm_sync_enum reply = slave[id]->nb_transport_bw(*trans, phase, t);
		}
	}

	/* 6DRS per 2flits */
	else if (rack_queue.size() >= 6) {
		
		/* CXL Latency (2flits) */
		for (int i = 0; i < 2; i++) {
			if (i == 0)
				wait(ctrl_latency, SC_NS);
			else
				wait(13, SC_NS);
			stats->increase_total_flit();
			flit_num++;
		}

		/* W/R flit together (8NDR+6DRS per 2flits) */
		if (!wack_queue.empty()) {
			/* MAX 8NDR */
			for (int i = 0; i < 8; i++) {
				tlm_generic_payload *trans = wack_queue.front();
				wack_queue.pop_front();
 				id = w_trans_id[trans->get_address()];
				tlm_sync_enum reply = slave[id]->nb_transport_bw(*trans, phase, t);
				if (wack_queue.empty())
					break;
			}
		}	
		
		/* 6DRS */	
		for (int i = 0; i < 6; i++) {
			tlm_generic_payload *trans = rack_queue.front();
			rack_queue.pop_front();
 			id = r_trans_id[trans->get_address()];
			tlm_sync_enum reply = slave[id]->nb_transport_bw(*trans, phase, t);
		}
	}

	/* Remaining R flit */
	else if ((rack_queue.size() == (req_num%6)) & (flit_num == (req_num/6)*2)) {
		wait(13, SC_NS);
		stats->increase_total_flit();
		flit_num = 0;
		for (int i = 0; i < (req_num%6); i++) {
			tlm_generic_payload *trans = rack_queue.front();
			rack_queue.pop_front();
 			id = r_trans_id[trans->get_address()];
			tlm_sync_enum reply = slave[id]->nb_transport_bw(*trans, phase, t);
		}
	}
}

tlm_sync_enum Controller::nb_transport_fw(int id, tlm_generic_payload& trans, tlm_phase& phase, sc_time& t) {
	/* READ */
	if (trans.get_command() == TLM_READ_COMMAND) {
		r_trans_id[trans.get_address()] = id;
		r_queue.push_back(&trans);
		r_num++;
	}

	/* WRITE */
	else {
		w_trans_id[trans.get_address()] = id;
		w_queue.push_back(&trans);
		w_num++;
	}
	
	return TLM_UPDATED;
}

tlm_sync_enum Controller::nb_transport_bw(int id, tlm_generic_payload& trans, tlm_phase& phase, sc_time& t) {
 	/* READ */
 	if (trans.get_command() == TLM_READ_COMMAND) {
 		id = r_trans_id[trans.get_address()];
		rack_queue.push_back(&trans);
 	}

	/* WRITE */
 	else {
 		id = w_trans_id[trans.get_address()];
		wack_queue.push_back(&trans);
 	}
	
	return TLM_UPDATED;
}
