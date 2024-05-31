#include "configurations.h"
#include <string>
#include <unistd.h>
#include <sysc/kernel/sc_event.h>
#include <utilities/sim_packet.h>
#include <utilities/statistics.h>
#include <utilities/shmem_communicator.h>

enum Status { INIT, RUNNING, BLOCKING, WAITING, TERMINATED };

class Host {
public:

    Host(const char* _name, int _unique_id);
    ~Host();
	
	void run_host_proc(int id);
    bool is_empty();
    int send_packet(Packet* pPacket);
    int recv_packet(Packet* pPacket);
    int irecv_packet(Packet* pPacket);
    void response_request(Packet* pPacket);
    void set_status(Status _status);
    Status get_status();

public:
    sc_core::sc_event packet_handled;
private:
    ShmemCommunicator *communicator;
    std::string name;
    int unique_id;
    Status 	status;
};
