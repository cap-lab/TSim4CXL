#include "dsp.h"

extern Configurations cfgs;

DSP::DSP(sc_module_name name, int id) : sc_module(name), master("master"), slave("slave"), clock("clock"),
										id(id), name(string(name)), stack(0), w_cycle(0), r_cycle(0)
{
	init();
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
			else if (stack == 8) {
				flit_packing_68(true);
			}

			/* WRITE */
			if(!wack_queue.empty() && w_cycle == 2) {
				flit_packing_68(false);
				w_cycle = 0;
			}
		}

		/* 256B Flit */
		else {
			/* WRITE */
			if (!wack_queue.empty() && ((w_cycle == 12) || (stats->get_write_flit_num() == req_num/12 && w_cycle == req_num%12))) {
				flit_packing_256(false);
				w_cycle = 0;
			}

			/* READ */
			if(!rack_queue.empty()) {
				flit_packing_256(true);
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
		if (stack == 8) {
			stack = 0;
			tlm_generic_payload *trans = pending_queue.front();	
			pending_queue.pop_front();

			/* CXL Latency (per flit) */
			wait(port_latency, SC_NS);
			stats->increase_read_flit();

			tlm_sync_enum reply = slave->nb_transport_bw(*trans, phase, t);
		}

		else {
			if (stack > 0) {
				tlm_generic_payload *trans = pending_queue.front();	
				pending_queue.pop_front();

				/* CXL Latency (per flit) */
				wait(port_latency, SC_NS);
				stats->increase_read_flit();

				tlm_sync_enum reply = slave->nb_transport_bw(*trans, phase, t);
			}
			tlm_generic_payload *trans = rack_queue.front();
			rack_queue.pop_front();
			pending_queue.push_back(trans);

			if (stack == 0) {
				/* CXL Latency (per flit) */
				wait(port_latency, SC_NS);
				stats->increase_read_flit();
			}

			stack++;
		}
	}

	/* WRITE */
	/* 2 NDR = 1 slot */
	else {
		/* CXL Latency (per flit) */
		wait(port_latency, SC_NS);
		stats->increase_write_flit();

		for (int i = 0; i < 2; i++) {
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
		if (stack < 2) {
			/* CXL Latency (per flit) */
			wait(port_latency, SC_NS);
			stats->increase_read_flit();

			for (int i = 0; i < 3; i++) {
				tlm_generic_payload *trans = rack_queue.front();
				rack_queue.pop_front();
				tlm_sync_enum reply = slave->nb_transport_bw(*trans, phase, t);

				if (rack_queue.empty())
					break;
			}
			stack++;
		}

		else {
			/* CXL Latency (per flit) */
			wait(port_latency, SC_NS);
			stats->increase_read_flit();

			for (int i = 0; i < 4; i++) {
				tlm_generic_payload *trans = rack_queue.front();
				rack_queue.pop_front();
				tlm_sync_enum reply = slave->nb_transport_bw(*trans, phase, t);

				if (rack_queue.empty())
					break;
			}
			stack = 0;
		}
	}

	/* WRITE */
	/* 12 NDR packed in a single flit */
	else {
		/* CXL Latency (per flit) */
		wait(port_latency, SC_NS);
		stats->increase_write_flit();

		for (int i = 0; i < 12; i++) {
			tlm_generic_payload *trans = wack_queue.front();
			wack_queue.pop_front();
			tlm_sync_enum reply = slave->nb_transport_bw(*trans, phase, t);

			if (wack_queue.empty())
				break;
		}
	}
}

tlm_sync_enum DSP::nb_transport_fw(int id, tlm_generic_payload& trans, tlm_phase& phase, sc_time& t) {
	if (phase == END_RESP)
		return TLM_COMPLETED;
	
	tlm_sync_enum reply = master->nb_transport_fw(trans, phase, t);

	return reply;
}

tlm_sync_enum DSP::nb_transport_bw(int id, tlm_generic_payload& trans, tlm_phase& phase, sc_time& t) {
	/* READ */
	if (trans.get_command() == TLM_READ_COMMAND) {
		rack_queue.push_back(&trans);
		r_cycle++;
	}

	/* WRITE */
	else {
		wack_queue.push_back(&trans);
		w_cycle++;
	}

	return TLM_UPDATED;
}
