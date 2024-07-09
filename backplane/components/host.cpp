#include "host.h"

Host::Host(string name, int id) : status(INIT), name(name), id(id), communicator(NULL) {}

Host::~Host() {
    if (communicator)
		delete communicator;
}

void Host::run_host_proc(int id) {
	communicator =  new ShmemCommunicator();
    communicator->prepare_connection(name.c_str(), id);

    /* Compile and execute the host process */
	stringstream command;
    command << "make clean -C ../../host/ && make -C ../../host/ && ../../host/host_exe " << id;

	pid_t pid = fork();
    
	if (pid == 0) {
    	int ret = system(command.str().c_str());
    	if (ret < 0)
    		cout << "Failed to execute the host process\n";
    	delete communicator;
    	exit(-1);
    }
    
	communicator->wait_connection();
	set_status(RUNNING);
}

int Host::recv_packet(Packet *pkt) {
    return communicator->recv_packet(pkt);
}

int Host::send_packet(Packet* pkt) {
    return communicator->send_packet(pkt);
}

int Host::response_request(Packet* pkt) {
    return send_packet(pkt);
}

void Host::set_status(Status _status){
    status = _status;
    if (_status != WAITING){
    	packet_handled.notify();
    }
}

Status Host::get_status(){
    return status;
}
