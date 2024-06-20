#ifndef ShmemCommunicator_H_
#define ShmemCommunicator_H_

#include "packet_buffer.h"
#include "sim_packet.h"
#include <time.h>
#include <sys/mman.h>
#include <pwd.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sstream>
#include <iostream>
#include <fstream>
#include <cassert>

using namespace std;

class ShmemCommunicator {
  public:
 
	ShmemCommunicator() {
		ts.tv_sec = 0;
		ts.tv_nsec = 1;
	};
    ~ShmemCommunicator();

    bool is_empty();
    int send_packet(Packet* pPacket);
    int irecv_packet(Packet* pPacket);
    int recv_packet(Packet* pPacket);
	void *establish_shm_segment(char *name, int size);
	bool prepare_connection(const char *name, int unique_id);
	void wait_connection();

  private:
	PacketBuffer *send_buffer;	//sending buffer
	char *bi_name;
    PacketBuffer *recv_buffer;	//receving buffer
	char *ib_name;

	struct timespec ts;
};

#endif
