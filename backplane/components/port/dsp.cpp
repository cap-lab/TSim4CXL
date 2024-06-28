#include "dsp.h"

extern Configurations cfgs;
extern map<uint32_t, uint32_t> addr_bst_map;

DSP::DSP(sc_module_name name, int id) : sc_module(name), master("master"), slave("slave"), clock("clock"),
		id(id), name(string(name)),
		w_msg(0), r_msg(0), w_flit(0), r_flit(0)
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

	/* Flit-Packing variables (payload num per flit) */	
	/* 68B/256B WRITE rsp (wo Data) */
	w_msg = (flit_mode == 68) ? 2 : (flit_mode == 256) ? 12 : w_msg;

	/* 256B READ rsp (w Data) */
	r_msg = 3;
}

void DSP::fw_thread() {
	tlm_phase phase = BEGIN_REQ;
	sc_time t = SC_ZERO_TIME;

	while(1) {
		if (!w_queue.empty()) {
			tlm_generic_payload *trans = w_queue.front();
			w_queue.pop_front();
			
			tlm_sync_enum reply = master->nb_transport_fw(*trans, phase, t);
		}

		if (!r_queue.empty()) {
			tlm_generic_payload *trans = r_queue.front();
			r_queue.pop_front();
	
			if (count_fw%8 == 0)
				wait(port_latency, SC_NS);
			else
				wait(link_latency, SC_NS);
			count_fw++;
			
			tlm_sync_enum reply = master->nb_transport_fw(*trans, phase, t);
		}	
		total_cycle++;
		wait(period, SC_NS);
	}
}

void DSP::bw_thread() {
	while(1) {
		/* 68B Flit */
		if (flit_mode == 68) {
			/* READ */
			if(!rack_queue.empty()) {
				flit_packing_68(true);
			}

			/* READ (Last Flit) */
			else if (r_flit == 8) {
				flit_packing_68(true);
			}

			/* WRITE */
			if(wack_queue.size() >= w_msg) {
				flit_packing_68(false);
			}
		}

		/* 256B Flit */
		else {
			/* WRITE (1st flit) */
			if ((w_flit == 0) && (wack_queue.size() >= w_msg)) {
				flit_packing_256(false);
			}
			
			/* WRITE (2nd flit) */
			else if ((w_flit > 0) && (wack_queue.size() >= req_num%w_msg)) {
				flit_packing_256(false);
				w_flit = 0;
			}
			
			/* READ (1st/2nd flit) */
			if((r_flit < 2) && (rack_queue.size() >= r_msg)) {
				flit_packing_256(true);
			}

			/* READ (3rd flit) */
			else if((r_flit == 2) && (rack_queue.size() >= r_msg+1)) {
				flit_packing_256(true);
				r_flit = 0;
			}
		}
		wait(period, SC_NS);
	}
}

void DSP::flit_packing_68(bool read) {
	tlm_phase phase = BEGIN_RESP;
	sc_time t = SC_ZERO_TIME;

	/* READ */
	if(read) {
		/* Last Flit */
		if (r_flit == 8) {
			r_flit = 0;
			tlm_generic_payload *trans = pending_queue.front();	
			pending_queue.pop_front();

			/* CXL Latency (per flit) */
			wait(21, SC_NS);
			stats->increase_r_flit();

			tlm_sync_enum reply = slave->nb_transport_bw(*trans, phase, t);
		}

		else {
			if (r_flit > 0) {
				tlm_generic_payload *trans = pending_queue.front();	
				pending_queue.pop_front();

				/* CXL Latency (per flit) */
				wait(21, SC_NS);
				stats->increase_r_flit();

				tlm_sync_enum reply = slave->nb_transport_bw(*trans, phase, t);
			}
			tlm_generic_payload *trans = rack_queue.front();
			rack_queue.pop_front();
			pending_queue.push_back(trans);

			if (r_flit == 0) {
				/* CXL Latency (per flit) */
				wait(port_latency+link_latency, SC_NS);
				stats->increase_r_flit();
			}

			r_flit++;
		}
	}

	/* WRITE */
	else {
		/* CXL Latency (per flit) */
		//wait(port_latency+link_latency, SC_NS);
		stats->increase_w_flit();

		for (int i = 0; i < w_msg; i++) {
			tlm_generic_payload *trans = wack_queue.front();
			wack_queue.pop_front();
			tlm_sync_enum reply = slave->nb_transport_bw(*trans, phase, t);
			if (wack_queue.empty())
				break;
		}
	}
}

void DSP::flit_packing_256(bool read) {
	tlm_phase phase = BEGIN_RESP;
	sc_time t = SC_ZERO_TIME;

	/* READ */
	if(read) {
		if (r_flit < 2) {
			r_flit++;

			/* CXL Latency (per flit) */
			if (r_flit == 0)
				wait(port_latency+link_latency, SC_NS);
			else
				wait(21, SC_NS);
			stats->increase_r_flit();
			
			if (stats->get_r_flit_num() % 5 == 0)
				r_flit = 0;

			//cout << "---------" << endl;	
			for (int i = 0; i < r_msg; i++) {
				tlm_generic_payload *trans = rack_queue.front();
				rack_queue.pop_front();

				//cout << name << "::(R_RESP)DSP->USP::" << trans->get_address() << endl;
				tlm_sync_enum reply = slave->nb_transport_bw(*trans, phase, t);

				if (rack_queue.empty())
					break;
			}
		}

		else {
			/* CXL Latency (per flit) */
			wait(21, SC_NS);
			stats->increase_r_flit();

			//cout << "---------" << endl;	
			for (int i = 0; i < (r_msg+1); i++) {
				tlm_generic_payload *trans = rack_queue.front();
				rack_queue.pop_front();

				//cout << name << "::(R_RESP)DSP->USP::" << trans->get_address() << endl;
				tlm_sync_enum reply = slave->nb_transport_bw(*trans, phase, t);
				if (rack_queue.empty())
					break;
			}
		}
	}

	/* WRITE */
	else {
		if (w_flit == 0) {
			/* CXL Latency (per flit) */
			//wait(port_latency, SC_NS);
			stats->increase_w_flit();
			w_flit++;

			//cout << "---------" << endl;	
			for (int i = 0; i < w_msg; i++) {
				tlm_generic_payload *trans = wack_queue.front();
				wack_queue.pop_front();
				//cout << name << "::(W_RESP)DSP->USP::" << trans->get_address() << endl;
				tlm_sync_enum reply = slave->nb_transport_bw(*trans, phase, t);
				if (wack_queue.empty()) {
					break;
				}
			}
		}
		else {
			/* CXL Latency (per flit) */
			//wait(port_latency, SC_NS);
			stats->increase_w_flit();
			w_flit++;

			//cout << "---------" << endl;	
			for (int i = 0; i < (req_num%w_msg); i++) {
				tlm_generic_payload *trans = wack_queue.front();
				wack_queue.pop_front();
				//cout << name << "::(W_RESP)DSP->USP::" << trans->get_address() << endl;
				tlm_sync_enum reply = slave->nb_transport_bw(*trans, phase, t);
				if (wack_queue.empty()) {
					break;
				}
			}
		}

	}
}

tlm_sync_enum DSP::nb_transport_fw(int id, tlm_generic_payload& trans, tlm_phase& phase, sc_time& t) {
	if (phase == END_RESP)
		return TLM_COMPLETED;

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
