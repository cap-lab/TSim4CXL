#include <string>
#include <unistd.h>
#include <sysc/kernel/sc_event.h>
#include <utilities/sim_packet.h>
#include <utilities/statistics.h>
#include <utilities/shmem_communicator.h>

enum Status { INIT, RUNNING, BLOCKING, WAITING, TERMINATED };

class Host {
public:

    Host(string name, int id);
    ~Host();
	
    Status get_status();
	void run_host_proc(int id);
    void set_status(Status _status);
    int send_packet(Packet* pkt);
    int recv_packet(Packet* pkt);
    int response_request(Packet* pkt);

public:
    sc_core::sc_event packet_handled;
private:
    ShmemCommunicator *communicator;
    int id;
    Status 	status;
	string name;
};
