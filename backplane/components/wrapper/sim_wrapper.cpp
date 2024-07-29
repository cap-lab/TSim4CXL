#include "sim_wrapper.h"

extern Configurations cfgs;
extern SyncObject sync_object;
extern uint32_t active_cores;
extern uint32_t active_dram;
map<tlm_generic_payload *, uint32_t> device_map;
map<uint32_t, uint32_t> addr_bst_map;

SIMWrapper::SIMWrapper(sc_module_name name, int id, int num) : sc_module(name), host_id(id), name(string(name)),
											active_cycle(0), total_cycle(0), outstanding(0), w_idx(0), r_idx(0), wack_num(0), rack_num(0),
											wack_incoming(NULL), rack_incoming(NULL), req_done(false), t(SC_ZERO_TIME),
											master("master"), clock("clock"), m_peq(this, &SIMWrapper::peq_cb)
{
	init();
    SC_THREAD(periodic_process);

    SC_METHOD(clock_posedge);
    sensitive << clock.pos();
    dont_initialize();

	SC_METHOD(clock_negedge);
    sensitive << clock.neg();
    dont_initialize();

	master.register_nb_transport_bw(this, &SIMWrapper::nb_transport_bw);
};

SIMWrapper::~SIMWrapper() {
	if (stats) {
		stats->print_stats();
		free(stats);
		stats = NULL;
	}
	if (active_cycle > 0) {
		cout << "> core-" << host_id << " Active Cycle  : " << active_cycle << " (cycles)\n";
		cout << "> core-" << host_id << " Total Cycle   : " << total_cycle << " (cycles)\n";
	}
    delete host;
}

void SIMWrapper::init() {
	host = new Host(name, host_id);
	stats = new Statistics();
	stats->set_name(name);

	period = cfgs.get_period(host_id);
	cpu_latency = cfgs.get_cpu_latency(host_id);
	packet_size = cfgs.get_packet_size();
	packet_data = new uint8_t[packet_size*sizeof(uint8_t)];
	dram_req_size = cfgs.get_dram_req_size();
}

void SIMWrapper::periodic_process() {
	/* Run host */
	host->run_host_proc(host_id);
   
	bool received = false;
	bool terminated = false;

   	/* Process the request every cycle */	
	Packet packet;

	while (true) {
		if (host->get_status() == RUNNING) {
			while (true) {
				if (host->recv_packet(&packet) != 0) {
					uint64_t delta = packet.delta;
					if (delta != 0) {
						cout << "H:" << host_id << " waiting for " << delta << "ns\n";
						wait(delta, SC_NS);
					}

					received = true;
					break;
				}

				/* Wait until the first packet is received */
				if(!received)
					continue;
				
				/* Proceed until the READ is done */	
				if (!req_done)
					wait(period, SC_NS);
			}
			handle_packet(&packet);
		}
		else if (host->get_status() == TERMINATED) {
			if (terminated == false) {
				active_cores--;
				terminated = true;
			}
		}
		if (host->get_status() == TERMINATED && active_dram == 0)
			break;
		
		wait(period, SC_NS);
    }
	cout << "Simulator stopped by Host-" << host_id << '\n'; 
	cout << "Read:" << (uint64_t)((sc_time_stamp() - read_start).to_double()/1000) << " ns"<< endl;
	sc_stop();
}

void SIMWrapper::forward_trans(tlm_generic_payload *trans) {
	tlm_phase phase = BEGIN_REQ;
	tlm_sync_enum reply = master->nb_transport_fw(*trans, phase, t);
	assert(reply == TLM_UPDATED);
	outstanding++;
}

void SIMWrapper::clock_posedge() {
	if (rack_incoming){
		rack_queue.push_back(rack_incoming);
		rack_incoming = NULL;
	}
	if (wack_incoming){
		wack_queue.push_back(wack_incoming);
		wack_incoming = NULL;
	}		
}

void SIMWrapper::clock_negedge() {
	/* READ */
    if (!r_queue.empty()) {
		tlm_generic_payload* trans = r_queue.front();
		r_queue.pop_front();
		forward_trans(trans);
	}
    
	/* WRITE */
    if (!w_queue.empty()) {
		tlm_generic_payload* trans = w_queue.front();
		w_queue.pop_front();
		forward_trans(trans);
	}

	/* Send RACK */
    if (!rack_queue.empty()) {
		tlm_generic_payload* trans = rack_queue.front();
		rack_queue.pop_front();	
		tlm_phase phase = END_RESP;

		tlm_sync_enum reply = master->nb_transport_fw(*trans, phase, t);
		assert(reply == TLM_COMPLETED);

		/* Accumulate the payload data */
		memcpy(packet_data + (dram_req_size*rack_num), trans->get_data_ptr(), trans->get_data_length());
		rack_num++;
		outstanding--;
    	
		/* Wait until the burst data condition is satisfied */
		if (rack_num == packet_size/dram_req_size)	{
			Packet packet;
			memset(&packet, 0, sizeof(Packet));
			packet.size = packet_size;
			memcpy(packet.data, packet_data, packet_size);
			
			/* Send response packet to the host */
			response_request(&packet);
			req_done = true;
			rack_num = 0;
		}	
	}

	/* Send WACK */
   	if (!wack_queue.empty()) {
    	tlm_generic_payload* trans = wack_queue.front();
		wack_queue.pop_front();	
		tlm_phase phase = END_RESP;

		tlm_sync_enum reply = master->nb_transport_fw(*trans, phase, t);
		assert(reply == TLM_COMPLETED);
		
		wack_num++;	
		outstanding--;
	
		/* Signal */
		if (wack_num == packet_size/dram_req_size) { 
			uint32_t sig = sync_queue.front();
			sync_object.signal(sig);
			sync_queue.pop_front();
			wack_num = 0;
		}
	}
	
	if (outstanding > 0) {
		active_cycle++;
	}
	total_cycle++;
}

void SIMWrapper::handle_packet(Packet *packet) {
    switch(packet->type) {
		case packet_read: 
			cout << "[W" << host_id << "]:[Pkt-Read-" << r_idx << "]\n";
			handle_read_packet(packet);
			r_idx++;
			break;
		case packet_write:
			cout << "[W" << host_id << "]:[Pkt-Write-" << w_idx << "]\n";
			handle_write_packet(packet);
			w_idx++;
			break;
		case packet_terminated:
		default:
			cout << "[W" << host_id << "]:[Pkt-Terminated]\n";
			update_status(TERMINATED);
			break;
    }
}

void SIMWrapper::handle_wait_packet(uint32_t addr) {
	bool sync_done = false;
	while (!sync_done) {
		sync_done = sync_object.check_signal(addr);
		wait(period, SC_NS);
	}
    sync_object.free(addr);
}

void SIMWrapper::handle_read_packet(Packet *packet) {
	/* Waiting for the signal from WACK */
	handle_wait_packet(packet->address);	
	wait(cpu_latency, SC_NS);

//	if (read_first) {
//		read_start = sc_time_stamp();
//		read_first = false;
//		wait(10000, SC_NS);
//	}
	
	req_done = false;
    uint32_t addr = packet->address;
	uint32_t device_id = packet->device_id;
	uint32_t burst_size = packet->size;
	
	stats->increase_r_packet();
	stats->update_total_read_size(packet_size);

	for (int i = 0; i < packet_size/dram_req_size; i++) {
		add_payload(TLM_READ_COMMAND, addr+(dram_req_size*i), dram_req_size, NULL, burst_size, device_id);
	}
}

void SIMWrapper::handle_write_packet(Packet *packet) {
	wait(cpu_latency, SC_NS);
	req_done = false;
	uint32_t addr = packet->address;
	uint32_t device_id = packet->device_id;
	uint32_t burst_size = packet->size;
	sync_queue.push_back(addr);
	stats->increase_w_packet();
	stats->update_total_write_size(packet_size);
	
	for (int i = 0; i < packet_size/dram_req_size; i++) {
		uint8_t* data = new uint8_t[dram_req_size];
		memcpy(data, (packet->data)+(dram_req_size*i), dram_req_size);		
		add_payload(TLM_WRITE_COMMAND, addr+(dram_req_size*i), dram_req_size, data, burst_size, device_id);
    }   
}

void SIMWrapper::add_payload(tlm_command cmd, uint32_t addr, uint32_t size, uint8_t *data, uint32_t burst_size, uint32_t device_id) {
	tlm_generic_payload* payload = m_mm.allocate();
	payload->acquire();
	payload->set_address(addr);
	payload->set_command(cmd);
	payload->set_data_length(size);
	payload->set_id(host_id);
	
	device_map[payload] = device_id;
	addr_bst_map[addr] = burst_size;
	
	if (payload->get_command() == TLM_READ_COMMAND) {
		r_queue.push_back(payload);
	}
	else {
		payload->set_data_ptr(data);
		w_queue.push_back(payload);
	}
}

tlm_sync_enum SIMWrapper::nb_transport_bw(int id, tlm_generic_payload& payload, tlm_phase& phase, sc_time& t) {
    m_peq.notify(payload, phase, t);
	return TLM_UPDATED;
}

void SIMWrapper::peq_cb(tlm_generic_payload& trans, const tlm_phase& phase) {
	/* Send RACK */
	if (trans.get_command() == TLM_READ_COMMAND)
		rack_incoming = &trans;

	/* Send WACK */
	else
		wack_incoming = &trans;
}

void SIMWrapper::response_request(Packet *packet) {
	host->response_request(packet);
}

void SIMWrapper::update_status(Status _status) {
    host->set_status(_status);
}
