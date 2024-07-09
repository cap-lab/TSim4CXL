#include "mem_wrapper.h"

extern Configurations cfgs;
extern uint32_t active_cores;
extern uint32_t active_dram;

MEMWrapper::MEMWrapper(sc_module_name name, string config_name, int id) : sc_module(name), slave("slave"), clock("clock"), 
													m_outstanding(0), id(id), mem_data(NULL), t(SC_ZERO_TIME),
													name(config_name), m_peq(this, &MEMWrapper::peq_cb)
{
	init();

	SC_THREAD(simulate_dram);

	SC_METHOD(clock_posedge);
    sensitive << clock.pos();
	dont_initialize();

	SC_METHOD(clock_negedge);
    sensitive << clock.neg();
	dont_initialize();
	
	slave.register_nb_transport_fw(this, &MEMWrapper::nb_transport_fw);
}

MEMWrapper::~MEMWrapper() {
	if (stats) {
		stats->print_mem_stats();
		free(stats);
		stats = NULL;
    }
	
	if (mem_data)
		free(mem_data);

	if (bridge)
		delete bridge;
}

void MEMWrapper::init() {
	stats = new Statistics();
	stats->set_name("Ramulator_" + name);
	string ramulator_path(RAMULATOR_PATH);
    string ramulator_cfg_path = ramulator_path + "configs/" + name + ".yaml";
	bridge = new Bridge(ramulator_cfg_path.c_str(), id);
	init_memdata();
}

void MEMWrapper::init_memdata() {
	if (mem_data == NULL) {
		uint64_t dram_size = cfgs.get_dram_size(id);
		mem_data = (uint8_t *) malloc(sizeof(uint8_t) * dram_size);
		memset(mem_data, 0, sizeof(uint8_t) * dram_size);
	}
}

void MEMWrapper::clock_posedge() {
    /* Read */
    if (!r_queue.empty()) {
		tlm_generic_payload *trans = r_queue.front();
		r_queue.pop_front();
		mem_request.push_back(trans);
    }

    /* Write */
    if (!w_queue.empty()) {
		tlm_generic_payload *trans = w_queue.front();
		w_queue.pop_front();
		mem_request.push_back(trans);
    }
}

void MEMWrapper::clock_negedge() {
	/* RACK */
	if (!rack_queue.empty())  {
		tlm_generic_payload* trans = rack_queue.front();
		rack_queue.pop_front();
		backward_trans(trans, true);
		m_outstanding--;
	}

	/* WACK */
	if (!wack_queue.empty()) {
		tlm_generic_payload* trans = wack_queue.front();
		wack_queue.pop_front();
		backward_trans(trans, false);
		m_outstanding--;
	}
}

tlm_generic_payload* MEMWrapper::gen_trans(uint64_t addr, tlm_command cmd, uint32_t size, uint32_t id) {
    tlm_generic_payload *trans;
    trans = m_mm.allocate();
    trans->acquire();
    trans->set_address(addr);
    trans->set_command(cmd);
    trans->set_data_length(size);
	trans->set_id(id);

    return trans;
}

tlm_sync_enum MEMWrapper::nb_transport_fw(tlm_generic_payload& trans, tlm_phase& phase, sc_time& t) {
    m_peq.notify(trans, phase, t);
	
	return TLM_UPDATED;
}

void MEMWrapper::simulate_dram() {
    double period = 1000.0 / cfgs.get_dram_freq(id);

	/* Simulate one cycle for ramulator and check if there's any completed payload */
	while (1) {
    	tlm_generic_payload *trans = NULL;

		/* Get a command which has finished in the DRAM simulator */
		trans = bridge->getCompletedCommand();

		if (trans) {
			if (trans->get_command() == TLM_READ_COMMAND) {
				rack_queue.push_back(trans);
				update_trans_delay(trans->get_address(), true);
			}
			else {
				wack_queue.push_back(trans);
				update_trans_delay(trans->get_address(), false);
			}
		}
		
		/* Send command to the DRAM simulator */
		while (mem_request.size() > 0) {
			trans = mem_request.front();
			mem_request.pop_front();
			
			if (trans->get_command() == TLM_READ_COMMAND) {
				stats->increase_r_request();
				stats->update_total_read_size(trans->get_data_length());
				r_trans_map[trans->get_address()] = (int) (sc_time_stamp().to_double() / 1000);
			}

			else {
				stats->increase_w_request();
				stats->update_total_write_size(trans->get_data_length());
				w_trans_map[trans->get_address()] = (int) (sc_time_stamp().to_double() / 1000);
			}

			bridge->sendCommand(*trans);
			m_outstanding++;
		}

		/* Simulate the DRAM simulator */
		bridge->ClockTick();
		
		if (active_cores == 0 && m_outstanding == 0) {
			active_dram = 0;
			break;
		}
		wait(period, SC_PS);
		total_cycle++;
    }
}

void MEMWrapper::backward_trans(tlm_generic_payload* trans, bool read) {
	tlm_phase phase = BEGIN_RESP;
	
	/* Read payload data from the backing store */
	if (read)
		trans->set_data_ptr(&(mem_data[trans->get_address()]));

	tlm_sync_enum reply = slave->nb_transport_bw(*trans, phase, t);
	assert(reply == TLM_UPDATED);
}

void MEMWrapper::peq_cb(tlm_generic_payload& trans, const tlm_phase& phase) {
	/* Generate a new payload for DRAM simulation */
	tlm_generic_payload *dram_trans = gen_trans(trans.get_address(), trans.get_command(), trans.get_data_length(), trans.get_id());

	if (phase == BEGIN_REQ) {
		/* Read */
		if (trans.get_command() == TLM_READ_COMMAND) {
			r_queue.push_back(dram_trans);
		}
		/* Write */
		else {
			/* Write data to the backing store */
			memcpy(mem_data+trans.get_address(), trans.get_data_ptr(), trans.get_data_length());
			w_queue.push_back(dram_trans);

		}	
	}	
	trans.release();
}

void MEMWrapper::update_trans_delay(uint32_t addr, bool is_read) {
	if (is_read) {
		uint32_t delay = (int)(sc_time_stamp().to_double()/1000) - r_trans_map[addr];
		stats->update_read_latency(delay);
	}
	else {
		uint32_t delay = (int)(sc_time_stamp().to_double()/1000) - w_trans_map[addr];
		stats->update_write_latency(delay);
	}
}
