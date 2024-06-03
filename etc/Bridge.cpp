#include "Bridge.h"

using namespace ramulator2;

int sc_main(int argc,char **argv) { return 0; }

Bridge::Bridge(const char *config_path, int id) {
	YAML::Node config = Ramulator::Config::parse_config_file(config_path, {});

	// This is one of the top-level objects in Ramulator 2.0.
	frontend = Ramulator::Factory::create_frontend(config);
	memory_system = Ramulator::Factory::create_memory_system(config);
	frontend->connect_memory_system(memory_system);
	memory_system->connect_frontend(frontend);
}

Bridge::~Bridge() {
	frontend->finalize();
	memory_system->finalize();
}

void Bridge::ClockTick() {
	// Callback function (When the request is done at DRAM)
	auto cmd_complete = [this](Ramulator::Request& req) { this->out_queue.q.push_back(req.trans); };

	if (in_queue.size() > 0) {
		tlm_generic_payload *trans = in_queue.q.front();
		int64_t address = static_cast<int64_t>(trans->get_address());
		
		// Generate request	
		Ramulator::Request req(address, trans->get_command(), 0, cmd_complete, trans);
		
		// Send request
		enqueue_success = memory_system->send(req);

		if (enqueue_success)
			in_queue.q.pop_front();
	}

	memory_system->tick();
}

void Bridge::sendCommand(tlm_generic_payload& trans) {
    in_queue.q.push_back(&trans);
}

tlm_generic_payload* Bridge::getCompletedCommand(void) {
    tlm_generic_payload *trans = NULL;

    if (out_queue.size() > 0) {
		trans = out_queue.q.front();
		out_queue.q.pop_front();
    }
    return trans;
}
