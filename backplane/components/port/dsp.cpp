#include "dsp.h"

extern Configurations cfgs;
extern map<uint32_t, uint32_t> addr_bst_map;

DSP::DSP(sc_module_name name, int id) : sc_module(name), master("master"), slave("slave"), clock("clock"),
		id(id), name(string(name)), w_msg(0), flit_num(0), fw_cnt(0), t(SC_ZERO_TIME)
{
	init();
	SC_THREAD(fw_thread);
	SC_THREAD(bw_thread);
	master.register_nb_transport_bw(this, &DSP::nb_transport_bw);
	slave.register_nb_transport_fw(this, &DSP::nb_transport_fw);
}

DSP::~DSP() {
	if (stats) {
		stats->print_stats();
		free(stats);
		stats = NULL;
	}
}

void DSP::init() {
	period = cfgs.get_period(id);
	flit_mode = cfgs.get_flit_mode();
	port_latency = cfgs.get_port_latency();
	link_latency = cfgs.get_link_latency();
	req_num = cfgs.get_packet_size()/cfgs.get_dram_req_size();
	stats = new Statistics();
	stats->set_name(name);

	/* Flit-Packing variables */	
	w_msg = (flit_mode == 68) ? 2 : (flit_mode == 256) ? 12 : w_msg;
}

void DSP::fw_thread() {
	tlm_phase phase = BEGIN_REQ;

	while(1) {
		if (!w_queue.empty()) {
			tlm_generic_payload *trans = w_queue.front();
			w_queue.pop_front();
			
			tlm_sync_enum reply = master->nb_transport_fw(*trans, phase, t);
		}

		if (!r_queue.empty()) {
			tlm_generic_payload *trans = r_queue.front();
			r_queue.pop_front();
	
			if (fw_cnt%8 == 0)
				wait(port_latency, SC_NS);
			else
				wait(link_latency, SC_NS);
			fw_cnt++;
			
			tlm_sync_enum reply = master->nb_transport_fw(*trans, phase, t);
		}	
		wait(period, SC_NS);
	}
}

void DSP::bw_thread() {
	while(1) {
		flit_mode == 68 ? flit_packing_68() : flit_packing_256();
		wait(period, SC_NS);
	}
}

void DSP::flit_packing_68() {
	tlm_phase phase = BEGIN_RESP;
	
	/* Only W flit */
	if (wack_queue.size() >= w_msg) {
		/* CXL Latency (per flit) */
		//wait(port_latency+link_latency, SC_NS);
		stats->increase_total_flit();

		for (int i = 0; i < w_msg; i++) {
			tlm_generic_payload *trans = wack_queue.front();
			wack_queue.pop_front();
			tlm_sync_enum reply = slave->nb_transport_bw(*trans, phase, t);
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
			tlm_sync_enum reply = slave->nb_transport_bw(*trans, phase, t);
		}
		
		/* 4DRS */	
		for (int i = 0; i < 4; i++) {
			wait(period, SC_NS);
			
			if (i == 0)
				wait(port_latency+link_latency, SC_NS);	
			else
				wait(13, SC_NS);

			tlm_generic_payload *trans = rack_queue.front();
			rack_queue.pop_front();
			tlm_sync_enum reply = slave->nb_transport_bw(*trans, phase, t);
		}
	}
}

void DSP::flit_packing_256() {
	tlm_phase phase = BEGIN_RESP;

	/* W flit only */
	if (wack_queue.size() >= w_msg) {
		/* CXL Latency (per flit) */
		//wait(port_latency+link_latency, SC_NS);
		stats->increase_total_flit();
		
		for (int i = 0; i < w_msg; i++) {
			tlm_generic_payload *trans = wack_queue.front();
			wack_queue.pop_front();
			tlm_sync_enum reply = slave->nb_transport_bw(*trans, phase, t);
		}
	}

	/* 6DRS per 2flits */
	else if (rack_queue.size() >= 6) {
		
		/* CXL Latency (2flits) */
		for (int i = 0; i < 2; i++) {
			//wait(port_latency+link_latency, SC_NS);
			stats->increase_total_flit();
			flit_num++;
		}

		/* W/R flit together (8NDR+6DRS per 2flits) */
		if (!wack_queue.empty()) {
			/* MAX 8NDR */
			for (int i = 0; i < 8; i++) {
				tlm_generic_payload *trans = wack_queue.front();
				wack_queue.pop_front();
				tlm_sync_enum reply = slave->nb_transport_bw(*trans, phase, t);
				if (wack_queue.empty())
					break;
			}
		}	
		
		/* 6DRS */	
		for (int i = 0; i < 6; i++) {
			tlm_generic_payload *trans = rack_queue.front();
			rack_queue.pop_front();
			tlm_sync_enum reply = slave->nb_transport_bw(*trans, phase, t);
		}
	}

	/* Remaining R flit */
	else if ((rack_queue.size() == (req_num%6)) & (flit_num == (req_num/6)*2)) {
		stats->increase_total_flit();
		flit_num = 0;
		for (int i = 0; i < (req_num%6); i++) {
			tlm_generic_payload *trans = rack_queue.front();
			rack_queue.pop_front();
			tlm_sync_enum reply = slave->nb_transport_bw(*trans, phase, t);
		}
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
