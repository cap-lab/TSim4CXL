#include "usp.h"

extern Configurations cfgs;

USP::USP(sc_module_name name, int id) : sc_module(name), master("master"), slave("slave"), clock("clock"),
										id(id), name(string(name)), stack(0), r_cycle(0), w_cycle(0)
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
			else if (stack == 4) {
				flit_packing_68(false);
			}

			/* READ */
			if(!r_queue.empty() && r_cycle == 2) {
				flit_packing_68(true);
				r_cycle = 0;
			}
		}

		/* 256B Flit */
		else {
			/* WRITE */
			if (!w_queue.empty() && ((w_cycle == 3) || (stats->get_write_flit_num() == req_num/3 && w_cycle == req_num%3))) {
				flit_packing_256(false);
				w_cycle = 0;
			}

			/* READ */
			if(!r_queue.empty() && r_cycle == 8) {
				flit_packing_256(true);
				r_cycle = 0;
			}
		}
		wait(period, SC_NS);
	}
}

void USP::flit_packing_68(bool read) {
	tlm_phase phase = BEGIN_REQ;
	sc_time t = SC_ZERO_TIME;

	/* READ */
	/* 2 Req packed in a single flit */
	if(read) { 
		/* CXL Latency (per flit) */
		wait(port_latency, SC_NS);
		stats->increase_read_flit();

		for (int i = 0; i < 2; i++) {
			tlm_generic_payload *trans = r_queue.front();
			r_queue.pop_front();
			tlm_sync_enum reply = master->nb_transport_fw(*trans, phase, t);

			if (r_queue.empty())
				break;
		}
	}

	/* WRITE */
	/* RwD with 4 data slots */
	else {
		/* Last Flit */
		if (stack == 4) {
			stack = 0;
			tlm_generic_payload *trans = pending_queue.front();	
			pending_queue.pop_front();
			
			/* CXL Latency (per flit) */
			wait(port_latency, SC_NS);
			stats->increase_write_flit();

			tlm_sync_enum reply = master->nb_transport_fw(*trans, phase, t);
		}

		else {
			if (stack > 0) {
				tlm_generic_payload *trans = pending_queue.front();	
				pending_queue.pop_front();
				
				/* CXL Latency (per flit) */
				wait(port_latency, SC_NS);
				stats->increase_write_flit();
				
				tlm_sync_enum reply = master->nb_transport_fw(*trans, phase, t);
			}
			tlm_generic_payload *trans = w_queue.front();
			w_queue.pop_front();
			pending_queue.push_back(trans);

			if (stack == 0) {
				/* CXL Latency (per flit) */
				wait(port_latency, SC_NS);
				stats->increase_write_flit();
			}

			stack++;
		}
	}
}

void USP::flit_packing_256(bool read) {
	tlm_phase phase = BEGIN_REQ;
	sc_time t = SC_ZERO_TIME;

	/* READ */
	/* 8 Reqs packed in a single flit */
	if(read) { 
		/* CXL Latency (per flit) */
		wait(port_latency, SC_NS);
		stats->increase_read_flit();

		for (int i = 0; i < 8; i++) {
			tlm_generic_payload *trans = r_queue.front();
			r_queue.pop_front();
			tlm_sync_enum reply = master->nb_transport_fw(*trans, phase, t);

			if (r_queue.empty())
				break;
		}
	}

	/* WRITE */
	/* 3 RwD packed in a single flit */
	else {
		/* CXL Latency (per flit) */
		wait(port_latency, SC_NS);
		stats->increase_write_flit();
		cout << "FLIT PACKING----" << endl;
		for (int i = 0; i < 3; i++) {
			tlm_generic_payload *trans = w_queue.front();
			w_queue.pop_front();
			cout << "Trans:" << trans->get_address() << endl;
			tlm_sync_enum reply = master->nb_transport_fw(*trans, phase, t);

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
		r_cycle++;		
	}

	/* WRITE */
	else {
		w_queue.push_back(&trans);
		cout << "Push back to w_queue::" << trans.get_address() << endl;
		w_cycle++;
	}

	return TLM_UPDATED;
}

tlm_sync_enum USP::nb_transport_bw(int id, tlm_generic_payload& trans, tlm_phase& phase, sc_time& t) {
	tlm_sync_enum reply = slave->nb_transport_bw(trans, phase, t);

	return reply;
}
