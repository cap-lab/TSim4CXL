#include "mem_wrapper.h"

extern Configurations cfgs;
extern uint32_t active_cores;
extern uint32_t active_dram;

MEMWrapper::MEMWrapper(sc_module_name name, string config_name, int id) : sc_module(name), slave("slave"), clock("clock"), 
													m_outstanding(0), id(id), mem_data(NULL), 
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
	string ramulator_config_path = ramulator_path + "configs/";
    ramulator_config_path = ramulator_config_path + name + ".yaml";
	bridge = new Bridge(ramulator_config_path.c_str(), id);
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
		tlm_generic_payload *r_outgoing = r_queue.front();
        r_queue.pop_front();
		mem_request.push_back(r_outgoing);
    }
    /* Write */
    if (!w_queue.empty()) {
		tlm_generic_payload *w_outgoing = w_queue.front();
        w_queue.pop_front();
		mem_request.push_back(w_outgoing);
    }
}

void MEMWrapper::clock_negedge() {
	if (!rack_queue.empty())  {
		tlm_generic_payload* payload = rack_queue.front();
		rack_queue.pop_front();
		respond_read_request(payload);
		m_outstanding--;
	}

	if (!wack_queue.empty()) {
		tlm_generic_payload* payload = wack_queue.front();
		wack_queue.pop_front();
		respond_write_request(payload);
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
			}
			else {
				wack_queue.push_back(trans);	
			}
		}
		
		/* Send command to the DRAM simulator */
		while (mem_request.size() > 0) {
			trans = mem_request.front();
			mem_request.pop_front();
			if (trans->get_command() == TLM_READ_COMMAND) {
				stats->increase_r_request();
				stats->update_total_read_size(trans->get_data_length());
				r_trans_map[trans] = (int) (sc_time_stamp().to_double() / 1000);
			}
			else {
				stats->increase_w_request();
				stats->update_total_write_size(trans->get_data_length());
				w_trans_map[trans] = (int) (sc_time_stamp().to_double() / 1000);
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

void MEMWrapper::respond_read_request(tlm_generic_payload *outgoing) {
	uint64_t addr = outgoing->get_address();
	tlm_phase phase = BEGIN_RESP;
	sc_time t = SC_ZERO_TIME;

	/* Read payload data from the backing store */
	outgoing->set_data_ptr(&(mem_data[addr]));

	tlm_sync_enum reply = slave->nb_transport_bw(*outgoing, phase, t);
	assert(reply == TLM_UPDATED);
	
}

void MEMWrapper::respond_write_request(tlm_generic_payload *outgoing) {
	tlm_phase phase = BEGIN_RESP;
	sc_time t = SC_ZERO_TIME;
	tlm_sync_enum reply = slave->nb_transport_bw(*outgoing, phase, t);
	assert(reply == TLM_UPDATED);
}

void MEMWrapper::update_payload_delay(tlm_generic_payload *payload, bool is_read) {
	map<tlm_generic_payload *, uint32_t> *trans_map;
    if (is_read)
        trans_map = &r_trans_map;
    else
        trans_map = &w_trans_map;
	map<tlm_generic_payload *, uint32_t>::iterator it_d = trans_map->find(payload);
    if (it_d != trans_map->end()) {
		uint32_t delay = (int) (sc_time_stamp().to_double() / 1000) - it_d->second;
		if (is_read) 
        	stats->update_read_latency(delay);
		else
        	stats->update_write_latency(delay);
	}
}

void MEMWrapper::peq_cb(tlm_generic_payload& trans, const tlm_phase& phase) {
	/* Generate payload for simulation */
	tlm_generic_payload *sim_trans = gen_trans(trans.get_address(), trans.get_command(), trans.get_data_length(), trans.get_id());
	if (phase == tlm::BEGIN_REQ) {
		/* Read */
		if (trans.get_command() == TLM_READ_COMMAND)
			r_queue.push_back(sim_trans);
		/* Write */
		else {
			/* Write payload data to the backing store */
			memcpy(mem_data+trans.get_address(), trans.get_data_ptr(), trans.get_data_length());
			w_queue.push_back(sim_trans);
		}	
	}	
}
