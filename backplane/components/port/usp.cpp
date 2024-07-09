#include "usp.h"

extern Configurations cfgs;
extern map<uint32_t, uint32_t> addr_bst_map;

USP::USP(sc_module_name name, int id) : sc_module(name), master("master"), slave("slave"), clock("clock"),
		id(id), name(string(name)), t(SC_ZERO_TIME),
		f_idx(0), w_msg(0), r_msg(0), w_flit_stack(0), remainder(0), last_flit(0), fw_cnt(0), bw_cnt(0)

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
	flit_mode = cfgs.get_flit_mode();
	port_latency = cfgs.get_port_latency();
	link_latency = cfgs.get_link_latency();
	dram_req_size = cfgs.get_dram_req_size();
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
			if(r_queue.size() >= r_msg) {
				flit_packing_68(true);
			}
		}

		/* 256B Flit */
		else {
			/* WRITE (1st flit) */
			if (w_queue.size() >= w_msg) {
				flit_packing_256(false);
			}
			
			/* WRITE (2nd flit) */
			if (!w_queue.empty()) {
				if ((w_queue.size() >= remainder) && (w_flit_stack == last_flit)) {
					flit_packing_256(false);
					w_flit_stack = 0;
				}
			}

			/* READ */
			if(r_queue.size() >= r_msg) {
				flit_packing_256(true);
			}
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

			tlm_sync_enum reply = slave->nb_transport_bw(*trans, phase, t);
		}
		if (!rack_queue.empty()) {
			tlm_generic_payload *trans = rack_queue.front();	
			rack_queue.pop_front();
		
			if (bw_cnt%5 == 0)
				wait(port_latency, SC_NS);
			else
				wait(13, SC_NS);
			bw_cnt++;	

			tlm_sync_enum reply = slave->nb_transport_bw(*trans, phase, t);
		}	
		wait(period, SC_NS);
	}
}

void USP::flit_packing_68(bool read) {
	tlm_phase phase = BEGIN_REQ;

	/* READ */
	if(read) { 
		/* CXL Latency (per flit) */
		stats->increase_r_flit();

		if (fw_cnt%8 == 0)
			wait(port_latency+link_latency, SC_NS);
		else
			wait(link_latency, SC_NS);
		fw_cnt++;

		for (int i = 0; i < r_msg; i++) {
			tlm_generic_payload *trans = r_queue.front();
			r_queue.pop_front();
			tlm_sync_enum reply = master->nb_transport_fw(*trans, phase, t);
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
			//wait(port_latency+link_latency, SC_NS);
			stats->increase_w_flit();
			
			tlm_sync_enum reply = master->nb_transport_fw(*trans, phase, t);
		}

		else {
			if (f_idx > 0) {
				tlm_generic_payload *trans = pending_queue.front();	
				pending_queue.pop_front();
				
				/* CXL Latency (per flit) */
				//wait(port_latency+link_latency, SC_NS);
				stats->increase_w_flit();
				
				tlm_sync_enum reply = master->nb_transport_fw(*trans, phase, t);
			}
			tlm_generic_payload *trans = w_queue.front();
			w_queue.pop_front();
			pending_queue.push_back(trans);

			if (f_idx == 0) {
				/* CXL Latency (per flit) */
				//wait(port_latency+link_latency, SC_NS);
				stats->increase_w_flit();
			}

			f_idx++;
		}
	}
}

void USP::flit_packing_256(bool read) {
	tlm_phase phase = BEGIN_REQ;

	/* READ */
	if(read) { 
		/* CXL Latency (per flit) */
		stats->increase_r_flit();
		
		if (fw_cnt%6 == 0)
			wait(port_latency+link_latency, SC_NS);
		else
			wait(link_latency, SC_NS);
		fw_cnt++;

		for (int i = 0; i < r_msg; i++) {
			tlm_generic_payload *trans = r_queue.front();
			r_queue.pop_front();
			tlm_sync_enum reply = master->nb_transport_fw(*trans, phase, t);
			if (r_queue.empty())
				break;
		}
	}

	/* WRITE */
	else {
		if (w_flit_stack == last_flit) {
			
			/* CXL Latency (per flit) */
			//wait(port_latency+link_latency, SC_NS);
			stats->increase_w_flit();
			w_flit_stack++;
			
			for (int i = 0; i < remainder; i++) {
				tlm_generic_payload *trans = w_queue.front();
				w_queue.pop_front();
				tlm_sync_enum reply = master->nb_transport_fw(*trans, phase, t);
				if (w_queue.empty())
					break;
			}
		}
		else {
			/* CXL Latency (per flit) */
			//wait(port_latency+link_latency, SC_NS);
			stats->increase_w_flit();
			w_flit_stack++;
			
			for (int i = 0; i < w_msg; i++) {
				tlm_generic_payload *trans = w_queue.front();
				w_queue.pop_front();
				tlm_sync_enum reply = master->nb_transport_fw(*trans, phase, t);
				if (w_queue.empty())
					break;
			}
		}
	}
}

tlm_sync_enum USP::nb_transport_fw(int id, tlm_generic_payload& trans, tlm_phase& phase, sc_time& t) {
	if (phase == END_RESP) {
		trans.release();
		return TLM_COMPLETED;
	}

	/* READ */
	if (trans.get_command() == TLM_READ_COMMAND) {
		r_queue.push_back(&trans);
	}

	/* WRITE */
	else {
		w_queue.push_back(&trans);
		if (flit_mode == 256) {
			uint32_t burst_size = addr_bst_map[trans.get_address()];
			last_flit = (burst_size/dram_req_size)/w_msg;
			remainder = (burst_size/dram_req_size)%w_msg;
		}
	}

	return TLM_UPDATED;
}

tlm_sync_enum USP::nb_transport_bw(int id, tlm_generic_payload& trans, tlm_phase& phase, sc_time& t) {
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
