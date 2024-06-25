#include "usp.h"

extern Configurations cfgs;

USP::USP(sc_module_name name, int id) : sc_module(name), master("master"), slave("slave"), clock("clock"),
										id(id), name(string(name)),
										f_idx(0), r_stack(0), w_stack(0), r_msg(0), w_msg(0)
{
	init();
	SC_THREAD(fw_thread);
	master.register_nb_transport_bw(this, &USP::nb_transport_bw);
	slave.register_nb_transport_fw(this, &USP::nb_transport_fw);
}

USP::~USP() {
	if (stats) {
		stats->print_stats();
		free(stats);
		stats = NULL;
	}
}

void USP::init() {
	period = cfgs.get_period(id);
	flit_mode = cfgs.get_flit_mode();
	port_latency = cfgs.get_port_latency();
	link_latency = cfgs.get_link_latency();
	req_num = cfgs.get_packet_size()/cfgs.get_dram_req_size();
	stats = new Statistics();
	stats->set_name(name);

	/* Flit-Packing variables (payload num per flit) */	
	/* 68B/256B READ req (wo Data) */
	r_msg = (flit_mode == 68) ? 2 : (flit_mode == 256) ? 8 : r_msg;
	/* 256B WRITE req (w Data) */
	w_msg = 3;
}

void USP::fw_thread() {
	while(1) {
		/* 68B Flit */
		if (flit_mode == 68) {
			/* WRITE */
			if(!w_queue.empty()) {
				flit_packing_68(false);
			}

			/* WRITE (Last Flit) */
			else if (f_idx == 4) {
				flit_packing_68(false);
			}

			/* READ */
			if(!r_queue.empty() && r_stack >= r_msg) {
				flit_packing_68(true);
			}
		}

		/* 256B Flit */
		else {
			/* WRITE */
			if (!w_queue.empty() && w_stack >= w_msg) {
				flit_packing_256(false);
			}
			
			/* WRITE (Last Flit) */	
			if (!w_queue.empty() && ((stats->get_w_flit_num()+1)%(req_num/w_msg+1) == 0 && w_stack >= req_num%w_msg)) {
				flit_packing_256(false);
			}

			/* READ */
			if(!r_queue.empty() && r_stack >= r_msg) {
				flit_packing_256(true);
			}
		}
		wait(period, SC_NS);
	}
}

void USP::flit_packing_68(bool read) {
	tlm_phase phase = BEGIN_REQ;
	sc_time t = SC_ZERO_TIME;

	/* READ */
	if(read) { 
		/* CXL Latency (per flit) */
		wait(port_latency+link_latency, SC_NS);
		stats->increase_r_flit();

		for (int i = 0; i < r_msg; i++) {
			tlm_generic_payload *trans = r_queue.front();
			r_queue.pop_front();
			tlm_sync_enum reply = master->nb_transport_fw(*trans, phase, t);
			r_stack--;
			if (r_queue.empty())
				break;
		}
	}

	/* WRITE */
	else {
		/* Last Flit */
		if (f_idx == 4) {
			f_idx = 0;
			tlm_generic_payload *trans = pending_queue.front();	
			pending_queue.pop_front();
			
			/* CXL Latency (per flit) */
			wait(port_latency+link_latency, SC_NS);
			stats->increase_w_flit();

			tlm_sync_enum reply = master->nb_transport_fw(*trans, phase, t);
		}

		else {
			if (f_idx > 0) {
				tlm_generic_payload *trans = pending_queue.front();	
				pending_queue.pop_front();
				
				/* CXL Latency (per flit) */
				wait(port_latency+link_latency, SC_NS);
				stats->increase_w_flit();
				
				tlm_sync_enum reply = master->nb_transport_fw(*trans, phase, t);
			}
			tlm_generic_payload *trans = w_queue.front();
			w_queue.pop_front();
			pending_queue.push_back(trans);

			if (f_idx == 0) {
				/* CXL Latency (per flit) */
				wait(port_latency+link_latency, SC_NS);
				stats->increase_w_flit();
			}

			f_idx++;
		}
	}
}

void USP::flit_packing_256(bool read) {
	tlm_phase phase = BEGIN_REQ;
	sc_time t = SC_ZERO_TIME;

	/* READ */
	if(read) { 
		/* CXL Latency (per flit) */
		wait(port_latency+link_latency, SC_NS);
		stats->increase_r_flit();

		for (int i = 0; i < r_msg; i++) {
			tlm_generic_payload *trans = r_queue.front();
			r_queue.pop_front();
			tlm_sync_enum reply = master->nb_transport_fw(*trans, phase, t);
			r_stack--;
			if (r_queue.empty())
				break;
		}
	}

	/* WRITE */
	else {
		/* CXL Latency (per flit) */
		wait(port_latency+link_latency, SC_NS);
		stats->increase_w_flit();
		
		for (int i = 0; i < w_msg; i++) {
			tlm_generic_payload *trans = w_queue.front();
			w_queue.pop_front();
			tlm_sync_enum reply = master->nb_transport_fw(*trans, phase, t);
			w_stack--;
			if (w_queue.empty())
				break;
		}
	}
}

tlm_sync_enum USP::nb_transport_fw(int id, tlm_generic_payload& trans, tlm_phase& phase, sc_time& t) {
	if (phase == END_RESP)
		return TLM_COMPLETED;
	
	/* READ */
	if (trans.get_command() == TLM_READ_COMMAND) {
		r_queue.push_back(&trans);
		r_stack++;		
	}

	/* WRITE */
	else {
		w_queue.push_back(&trans);
		w_stack++;
	}

	return TLM_UPDATED;
}

tlm_sync_enum USP::nb_transport_bw(int id, tlm_generic_payload& trans, tlm_phase& phase, sc_time& t) {
	tlm_sync_enum reply = slave->nb_transport_bw(trans, phase, t);

	return reply;
}
