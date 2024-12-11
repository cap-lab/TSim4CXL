#include "usp.h"

extern Configurations cfgs;

USP::USP(sc_module_name name, int id) : sc_module(name), master("master"), slave("slave"), clock("clock"),
		id(id), name(string(name)), t(SC_ZERO_TIME), f_idx(0), r_msg(0), rack_num(0), wack_num(0), pipeline(0)
{
	init();
	SC_THREAD(fw_thread);
	SC_THREAD(bw_thread);
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
	dram_num = cfgs.get_dram_num();
	port_latency = cfgs.get_port_latency();
	link_latency = cfgs.get_link_latency();
	payload_num = cfgs.get_packet_size()/cfgs.get_dram_req_size();

	stats = new Statistics();
	stats->set_name(name);

	r_msg = 2; /* Payload num per flit */
	pipeline = 8; /* Response pipeline cycle */
}

void USP::fw_thread() {
	while(1) {
		/* WRITE */
		if(!w_queue.empty()) {
			flit_packing_68(false);
		}

		/* WRITE (Last Flit) */
		else if (f_idx == 4) {
			flit_packing_68(false);
		}

		/* READ */
		if(r_queue.size() >= r_msg) {
			flit_packing_68(true);
		}

		wait(period, SC_NS);
	}
}

void USP::bw_thread() {
	tlm_phase phase = BEGIN_RESP;
	sc_time t = SC_ZERO_TIME;

	while(1) {
		if (!wack_queue.empty()) {
			tlm_generic_payload *trans = wack_queue.front();	
			wack_queue.pop_front();
			
			if (wack_num%pipeline==0) {
				wait(port_latency, SC_NS);
			}
			else {
				wait(link_latency, SC_NS);
			}

			tlm_sync_enum reply = slave->nb_transport_bw(*trans, phase, t);
			wack_num++;
		}
		if (!rack_queue.empty()) {
			tlm_generic_payload *trans = rack_queue.front();
			rack_queue.pop_front();
			
			if (rack_num%pipeline==0) {
				wait(port_latency, SC_NS);
			}
			else {
				wait(link_latency, SC_NS);
			}

			tlm_sync_enum reply = slave->nb_transport_bw(*trans, phase, t);
			rack_num++;
		}	
		wait(period, SC_NS);
	}
}

void USP::flit_packing_68(bool read) {
	tlm_phase phase = BEGIN_REQ;

	/* READ */
	if(read) { 
		/* CXL Latency (per flit) */
		if (stats->get_r_flit_num()%(payload_num/r_msg) == 0) {
			wait(port_latency+link_latency, SC_NS);
		} else {
			wait(period, SC_NS);
		}

		for (int i = 0; i < r_msg; i++) {
			tlm_generic_payload *trans = r_queue.front();
			r_queue.pop_front();
			tlm_sync_enum reply = master->nb_transport_fw(*trans, phase, t);
		}

		stats->increase_r_flit();
	}

	/* WRITE */
	else {
		/* Last Flit */
		if (f_idx == 4) {
			f_idx = 0;
			tlm_generic_payload *trans = pending_queue.front();	
			pending_queue.pop_front();
			
			/* CXL Latency (per flit) */
			wait(period, SC_NS);
			stats->increase_w_flit();
			
			tlm_sync_enum reply = master->nb_transport_fw(*trans, phase, t);
		}

		else {
			if (f_idx == 0) {
				/* CXL Latency (per flit) */
				if (stats->get_w_flit_num()%20 == 0) {
					wait(port_latency+link_latency, SC_NS);
				} else {
					wait(period, SC_NS);
				}

				stats->increase_w_flit();
			}

			else if (f_idx > 0) {
				tlm_generic_payload *trans = pending_queue.front();	
				pending_queue.pop_front();
				
				/* CXL Latency (per flit) */
				wait(period, SC_NS);
				stats->increase_w_flit();
				
				tlm_sync_enum reply = master->nb_transport_fw(*trans, phase, t);
			}
			tlm_generic_payload *trans = w_queue.front();
			w_queue.pop_front();
			pending_queue.push_back(trans);

			f_idx++;
		}
	}
}

tlm_sync_enum USP::nb_transport_fw(int id, tlm_generic_payload& trans, tlm_phase& phase, sc_time& t) {
	if (phase == END_RESP) {
		trans.release();
		return TLM_COMPLETED;
	}

	auto& queue = (trans.get_command() == TLM_READ_COMMAND) ? r_queue : w_queue;
    queue.push_back(&trans);

	return TLM_UPDATED;
}

tlm_sync_enum USP::nb_transport_bw(int id, tlm_generic_payload& trans, tlm_phase& phase, sc_time& t) {
	auto& queue = (trans.get_command() == TLM_READ_COMMAND) ? rack_queue : wack_queue;
    queue.push_back(&trans);

	return TLM_UPDATED;
}
