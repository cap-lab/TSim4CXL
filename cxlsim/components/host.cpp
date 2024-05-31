#include "host.h"

extern Configurations cfgs;

Host::Host(const char *_name, int _unique_id):status(INIT){
    name = std::string(_name);
    unique_id = _unique_id;
    communicator = NULL;
}

Host::~Host() {
    if (communicator)
        delete communicator;
}

void Host::run_host_proc(int id) {
	stringstream command;

    // Compile and Execute the C++ code
    command << "make clean -C ../../host/cpp/ && make -C ../../host/cpp/ && ../../host/cpp/my_program " << id;

	communicator =  new ShmemCommunicator();
    communicator->prepare_connection(name.c_str(), unique_id);
    
	pid_t pid = fork();
    
	if (pid == 0) {
        int ret = system(command.str().c_str());
        if (ret < 0)
            std::cout << "Failed to execute the host process" << std::endl;
        delete communicator;
        exit(-1);
    }
    
	communicator->wait_connection();
	
	set_status(RUNNING);
}

bool Host::is_empty() {
    return communicator->is_empty();
}

int Host::recv_packet(Packet *packet) {
    return communicator->recv_packet(packet);
}

int Host::irecv_packet(Packet *packet) {
    return communicator->irecv_packet(packet);
}

int Host::send_packet(Packet* pPacket) {
    return communicator->send_packet(pPacket);
}

void Host::response_request(Packet* packet) {
    send_packet(packet);
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
