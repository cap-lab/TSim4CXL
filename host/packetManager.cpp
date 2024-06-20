#include "packetManager.h"

traceManager::traceManager(int coreID, string name) {
    ts.tv_sec = 0;
    ts.tv_nsec = 1;
    prepare_connection(coreID, name);
}

bool traceManager::prepare_connection(int coreID, string name) {
    struct passwd *pd = getpwuid(getuid());  // Check for NULL!

    ib_name = new char[64];
	bi_name = new char[64];

	srand((unsigned int) time(0));

    sprintf(ib_name, "/%s_shmem_ib_%d_%d",pd->pw_name, coreID, 0);
    sprintf(bi_name, "/%s_shmem_bi_%d_%d",pd->pw_name, coreID, 0);

    stringstream filePath;
    filePath << "../../shared/.args.shmem.dat_" << coreID;
    ofstream ofs(filePath.str());
    assert(ofs.is_open());
    ofs << name << endl << ib_name << endl << bi_name << endl;
	ofs << "buffer_size " << PKT_BUFFER_SIZE << endl;
    ofs.close();

    sendBuffer = (PacketBuffer*)establish_shm_segment(ib_name, sizeof(PacketBuffer));
    memset(sendBuffer, 0x0, sizeof(PacketBuffer));
    recvBuffer = (PacketBuffer*)establish_shm_segment(bi_name, sizeof(PacketBuffer));
    memset(recvBuffer, 0x0, sizeof(PacketBuffer));

    pb_init(sendBuffer);
    pb_init(recvBuffer);

    if (!sendBuffer || !recvBuffer) {
        return false;
    }

    // For shared memory connection
    Packet packet;
    memset(&packet, 0, sizeof(Packet));
    packet.type = packet_write;
    sendPacket(&packet);

    return true;
}

void *traceManager::establish_shm_segment(char *name, int size) {
    int fd = shm_open(name, O_RDWR | O_CREAT, 0600);
    if (fd < 0)
        cerr << "shm_open fails with " << name << endl;
    if (ftruncate(fd, size) < 0)
        cerr << "ftruncate() shared memory segment" << endl;
    void *segment =	(void *) mmap(NULL, size, PROT_READ | PROT_WRITE,	MAP_SHARED, fd, 0);
    if (segment == MAP_FAILED)
        cerr << "mapping shared memory segment" << endl;
    close(fd);

    return segment;
}

bool traceManager::is_empty() {
    return pb_is_empty(recvBuffer);
}

int traceManager::sendPacket(Packet * pPacket) {
    while (pb_is_full(sendBuffer)){nanosleep(&ts, NULL); };
    pb_write(sendBuffer, pPacket);
	return sizeof(pPacket);
}

int traceManager::recvPacket(Packet * pPacket) {
    while (pb_is_empty(recvBuffer)) { nanosleep(&ts, NULL); };
	pb_read(recvBuffer, pPacket);
	return sizeof(pPacket);
}

void traceManager::readPacketData(uint8_t *data) {
    Packet r_packet;
    memset(&r_packet, 0, sizeof(Packet));
    recvPacket(&r_packet);
	memcpy(data, r_packet.data, r_packet.size);
}

void traceManager::readRequest(uint32_t address, uint32_t size, uint32_t device, uint64_t cycle) {
    Packet* packet = makePacket(packet_read, address, NULL, size, device, cycle);
    sendPacket(packet);
}

void traceManager::writeRequest(uint32_t address, uint8_t* data, uint32_t size, uint32_t device, uint64_t cycle) {
    Packet* packet = makePacket(packet_write, address, data, size, device, cycle);
	sendPacket(packet);
}

void traceManager::signalRequest(uint32_t signalID) {
    Packet* packet = makePacket(packet_bar_signal, signalID, NULL, 0, 0, 0);
    sendPacket(packet);    
}

void traceManager::waitRequest(uint32_t waitID) {
    Packet* packet = makePacket(packet_bar_wait, waitID, NULL, 0, 0, 0);
    sendPacket(packet);    
}

void traceManager::terminateRequest(uint64_t cycle) {
    Packet* packet = makePacket(packet_terminated, 0, NULL, 0, 0, cycle);
    sendPacket(packet);   
}

Packet* traceManager::makePacket(PacketType t, uint32_t address, uint8_t* data, uint32_t size, uint32_t device, uint64_t cycle) {
    Packet* packet = new Packet;
    memset(packet, 0, sizeof(Packet));
    packet->type = t;
    packet->size = size;
    packet->cycle = cycle;
    packet->address = address;
    packet->device_id = device;
    if (data != NULL)
        memcpy(packet->data, data, size);

    return packet;
}
