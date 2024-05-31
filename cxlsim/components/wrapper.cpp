#include "wrapper.h"

extern Configurations cfgs;
extern SyncObject sync_object;
extern uint32_t active_cores;
extern uint32_t active_dram;
map<tlm_generic_payload *, uint32_t> device_map;

Wrapper::Wrapper(sc_module_name name, int id, int num) : sc_module(name),
											active_cycle(0), total_cycle(0), outstanding(0),
											req_done(false), received(false),
                                            master("master"), clock("clock"), m_peq(this, &Wrapper::peq_cb)
{
	/* Host */
    host_id = id;
	host = new Host(name, host_id);
	
	terminate = false;
	flit_size = cfgs.get_flit_size();
	period = cfgs.get_period(id);
	cpu_latency = cfgs.get_cpu_latency(id);
	packet_data = new uint8_t[cfgs.get_packet_size()];

	/* Flit Packing */
	slots_per_flit = 4;
	flit_stack = 0;
	
    SC_THREAD(periodic_process);

    SC_METHOD(clock_posedge);
    sensitive << clock.pos();
    dont_initialize();

	SC_METHOD(clock_negedge);
    sensitive << clock.neg();
    dont_initialize();

	master.register_nb_transport_bw(this, &Wrapper::nb_transport_bw);
	
	stats = new Statistics();
	stats->set_name(string(name));

	w_idx = r_idx = rack_num = wack_num = 0;
};

Wrapper::~Wrapper() {
	if (stats) {
		stats->print_stats();
		free(stats);
		stats = NULL;
	}
	if (active_cycle > 0) {
		cout << "> core-" << host_id << " Active Cycle  : " << active_cycle << " (cycles)" << '\n';
		cout << "> core-" << host_id << " Total Cycle   : " << total_cycle << " (cycles)" << '\n';
	}
    delete host;
}

void Wrapper::periodic_process() {
	/* Run host application */
	host->run_host_proc(host_id);
   
   	/* Process the request every cycle */	
	Packet packet;
	while (true) {
        if (host->get_status() == RUNNING) {
            while (true) {
                if (host->irecv_packet(&packet) != 0) {
					uint64_t delta = packet.cycle;
					if (delta != 0)
						wait(delta, SC_NS);

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
			if (terminate == false) {
				active_cores--;
				terminate = true;
			}
			wait(period, SC_NS);
		}
		if (host->get_status() == TERMINATED && active_dram == 0)
			break;
		
		wait(period, SC_NS);
    }
	cout << "Simulator stopped by Host-" << host_id << '\n'; 
	sc_stop();
}

void Wrapper::send_read_request(tlm_generic_payload *outgoing) {
	tlm_phase phase = BEGIN_REQ;
	sc_time t = SC_ZERO_TIME;
	tlm_sync_enum reply = master->nb_transport_fw(*outgoing, phase, t);
	assert(reply == TLM_UPDATED);
	outstanding++;
}

void Wrapper::send_write_request(tlm_generic_payload *outgoing) {
	tlm_phase phase = BEGIN_REQ;
	sc_time t = SC_ZERO_TIME;
	tlm_sync_enum reply = master->nb_transport_fw(*outgoing, phase, t);
	assert(reply == TLM_UPDATED);
	outstanding++;
}

void Wrapper::clock_posedge() {
	tlm_generic_payload *incoming = NULL;
	if (wack_incoming.size() > 0){
		incoming = wack_incoming.front();
		wack_incoming.pop_front();
		wack_queue.push_back(incoming);
	}
	if (rack_incoming.size() > 0){
		incoming = rack_incoming.front();
		rack_incoming.pop_front();
		rack_queue.push_back(incoming);
	}		
}

void Wrapper::clock_negedge() {
	/* READ */
    if (!r_queue.empty())
		flit_packing(true);
    
	/* WRITE */
    if (w_queue.empty() && waiting) {
		send_write_request(waiting);
		flit_stack = 0;
		waiting = NULL;
		stats->increase_write_flit();
	}
	if (!w_queue.empty())
		flit_packing(false);

	/* Send RACK */
    if (!rack_queue.empty()) {
		tlm_generic_payload* payload = rack_queue.front();
		rack_queue.pop_front();	
		tlm_phase phase = END_RESP;
		sc_time t = SC_ZERO_TIME;

		tlm_sync_enum reply = master->nb_transport_fw(*payload, phase, t);
		assert(reply == TLM_COMPLETED);
		
		int burst = r_req_size/flit_size; 	
		uint8_t* d_data = payload->get_data_ptr();
		
		/* Accumulate the payload data */
		memcpy(packet_data + (flit_size*rack_num), d_data, payload->get_data_length());
		rack_num++;
		outstanding--;
		
    	/* Wait until the burst data condition is satisfied */
		if (rack_num == burst)	{
			Packet packet;
			memset(&packet, 0, sizeof(Packet));
			packet.address = 0;
			packet.size = payload->get_data_length() * burst;
			memcpy(packet.data, packet_data, r_req_size);
			
			/* Send response packet to the host */
			response_request(&packet);
			req_done = true;
			rack_num = 0;
		}	
	}

	/* Send WACK */
   	if (!wack_queue.empty()) {
    	tlm_generic_payload* payload = wack_queue.front();
		wack_queue.pop_front();	
		tlm_phase phase = END_RESP;
		sc_time t = SC_ZERO_TIME;
		wack_num++;	
		
		tlm_sync_enum reply = master->nb_transport_fw(*payload, phase, t);
		assert(reply == TLM_COMPLETED);
		outstanding--;
		
		/* SIGNAL(ready to read) */
		if (wack_num == w_req_size/flit_size) { 
			uint32_t sig = signal_queue.front();
			sync_object.signal(sig);
			signal_queue.pop_front();
			wack_num = 0;
		}
	}
	
	/* Synchronization */
	if (!sync_queue.empty() && outstanding == 0) {
		uint32_t sync_id = sync_queue.front();
		sync_object.signal(sync_id);
		sync_queue.pop_front();
	}
	if (outstanding > 0) {
		active_cycle++;
	}
	total_cycle++;
}

void Wrapper::flit_packing(bool is_read) {
	if (is_read) {
		for (int i = 0; i < slots_per_flit; i++) {
			tlm_generic_payload* payload = r_queue.front();
			r_queue.pop_front();
			send_read_request(payload);
			if (r_queue.empty())
				break;
		}
		stats->increase_read_flit();
	}
	else {
		tlm_generic_payload* payload = NULL;
		/* All-Data Flit */
		if (flit_stack == 4) {
			send_write_request(waiting);
			flit_stack = 0;
		}
		/* Normal Flit */
		else {
			payload = w_queue.front();  
			w_queue.pop_front();
			if (flit_stack > 0) 
				send_write_request(waiting);
			flit_stack++;
		}
		waiting = payload;
		stats->increase_write_flit();
	}
}

void Wrapper::handle_packet(Packet *packet) {
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
        case packet_bar_signal:
			cout << "[W" << host_id << "]:[Pkt-Signal]\n";
			handle_signal_packet(packet);
            break;
        case packet_bar_wait:
			cout << "[W" << host_id << "]:[Pkt-Wait]\n";
			handle_wait_packet(packet);
            break;
        case packet_terminated:
        default:
			cout << "[W" << host_id << "]:[Pkt-Terminated]\n";
			update_status(TERMINATED);
            break;
    }
}

void Wrapper::handle_wait_packet(Packet *packet) {
	bool sync_done = false;
	uint32_t sync_id = packet->address;
	while (!sync_done) {
		sync_done = sync_object.check_signal(sync_id);
		wait(period, SC_NS);
	}
    sync_object.free(sync_id);
}

void Wrapper::handle_signal_packet(Packet *packet) {
	uint32_t sync_id = packet->address;
	sync_queue.push_back(sync_id);
}

void Wrapper::handle_read_packet(Packet *packet) {
	/* Waiting for the signal from WACK */
	handle_wait_packet(packet);	

	wait(cpu_latency, SC_NS);
	
	req_done = false;
	r_req_size = packet->size; 
    uint32_t addr = packet->address;
	uint32_t device_id = packet->device_id;
	
	stats->increase_read_packet();
	stats->update_total_read_size(r_req_size);

	for (int i = 0; i < r_req_size/flit_size; i++)
		add_payload(TLM_READ_COMMAND, addr+(i*flit_size), flit_size, NULL, device_id);
}

void Wrapper::handle_write_packet(Packet *packet) {
	req_done = false;
	uint32_t addr = packet->address;
	uint32_t device_id = packet->device_id;
	w_req_size = packet->size; 

	signal_queue.push_back((int)addr);
	stats->increase_write_packet();
	stats->update_total_write_size(w_req_size);
	
	for (int i = 0; i < w_req_size/flit_size; i++) {
		uint8_t* data = new uint8_t[flit_size];
		memcpy(data, (packet->data)+(flit_size*i), flit_size);		
		add_payload(TLM_WRITE_COMMAND, addr+(i*flit_size), flit_size, data, device_id);
    }   
}

void Wrapper::add_payload(tlm_command cmd, uint32_t address, uint32_t size, uint8_t *data, uint32_t device_id) {
	tlm_generic_payload* payload = m_mm.allocate();
	payload->acquire();
	payload->set_address(address);
	payload->set_command(cmd);
	payload->set_data_length(size);
	payload->set_id(host_id);
	device_map[payload] = device_id;
	
	switch (payload->get_command()) {
		case TLM_READ_COMMAND:
			r_queue.push_back(payload);
			break;
		case TLM_WRITE_COMMAND:
			payload->set_data_ptr(data);
			w_queue.push_back(payload);
			break;
		default:
			sc_assert(!"Can only generate read and write traffic");
	}
}

tlm_sync_enum Wrapper::nb_transport_bw(int id, tlm_generic_payload& payload, tlm_phase& phase, sc_time& t) {
    m_peq.notify(payload, phase, t);
	return TLM_UPDATED;
}

void Wrapper::peq_cb(tlm_generic_payload& payload, const tlm_phase& phase) {
	if (phase == BEGIN_RESP) {
		/* Send RACK */
		if (payload.get_command() == TLM_READ_COMMAND)
			rack_queue.push_back(&payload);

		/* Send WACK */
		else
			wack_queue.push_back(&payload);
	}
}

void Wrapper::response_request(Packet *packet) {
	host->response_request(packet);
}

void Wrapper::update_status(Status _status) {
    host->set_status(_status);
}
