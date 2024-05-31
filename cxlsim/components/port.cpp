#include "port.h"

extern Configurations cfgs;

Port::Port(sc_module_name name, int id) : sc_module(name), id(id), total_cycle(0), master("master"), slave("slave") 
{
	port_name = string(name);

	master.register_nb_transport_bw(this, &Port::nb_transport_bw);
	slave.register_nb_transport_fw(this, &Port::nb_transport_fw);
	
	port_latency = cfgs.get_port_latency();
	link_latency = cfgs.get_link_latency();
	period = cfgs.get_period(0);

	SC_THREAD(delayed_transport);
}

Port::~Port() {}

void Port::delayed_transport() {
	while(1) {
		/* nb_transport_fw */
		if (!fw_queue.empty()) {
			tlm_phase phase = BEGIN_REQ;
			sc_time t = SC_ZERO_TIME;
		
			tlm_generic_payload *trans = fw_queue.front();
			if (trans->get_command() == TLM_READ_COMMAND) {
				if (count_fw%4==0)
					wait(port_latency, SC_NS);
				else
					wait(link_latency, SC_NS);
				count_fw++;

				for (int i = 0; i < 4; i++) {
					trans = fw_queue.front();
					fw_queue.pop_front();

					tlm_sync_enum reply = master->nb_transport_fw(*trans, phase, t);
					if (fw_queue.empty())
						break;
				}
			}
			else {	
				tlm_sync_enum reply = master->nb_transport_fw(*trans, phase, t);
				fw_queue.pop_front();
			}
		}
		/* nb_transport_bw */
		if (!bw_queue.empty()) {
			tlm_generic_payload *trans = bw_queue.front();
			int id;
			
			if (trans->get_command() == TLM_READ_COMMAND)
				id = r_trans_id[trans->get_address()];
			else
				id = w_trans_id[trans->get_address()];

			tlm_phase phase = BEGIN_RESP;
			sc_time t = SC_ZERO_TIME;
			
			if (trans->get_command() == TLM_READ_COMMAND) {
				if (count_bw%4==0)
					wait(port_latency, SC_NS);
				else
					wait(link_latency, SC_NS);
				
				count_bw++;
				tlm_sync_enum reply = slave[id]->nb_transport_bw(*trans, phase, t);
				bw_queue.pop_front();
			}

			if (trans->get_command() == TLM_WRITE_COMMAND) {
				for (int i = 0; i < 4; i++) {
					trans = bw_queue.front();
					bw_queue.pop_front();
					tlm_sync_enum reply = slave[id]->nb_transport_bw(*trans, phase, t);
					if (bw_queue.empty())
						break;
				}
			}
		}
		else {
			wait(period, SC_NS);
		}
	}
}

tlm_sync_enum Port::nb_transport_fw(int id, tlm_generic_payload& trans, tlm_phase& phase, sc_time& t) {
	if (phase == END_RESP)
		return TLM_COMPLETED;
	
	if (trans.get_command() == TLM_READ_COMMAND)
		r_trans_id[trans.get_address()] = id;
	else
		w_trans_id[trans.get_address()] = id;
	
	fw_queue.push_back(&trans);

	return TLM_UPDATED;
}

tlm_sync_enum Port::nb_transport_bw(int id, tlm_generic_payload& trans, tlm_phase& phase, sc_time& t) {
	bw_queue.push_back(&trans);
	
	return TLM_UPDATED;
}
