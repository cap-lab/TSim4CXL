#include "shmem_communicator.h"

ShmemCommunicator::~ShmemCommunicator() {
    int ret;

    munmap(send_buffer, sizeof(PacketBuffer));
    munmap(recv_buffer, sizeof(PacketBuffer));

    shm_unlink(ib_name);
    shm_unlink(bi_name);

    string del_file = "rm -rf /dev/shm/" + string(ib_name);

    ret = system(del_file.c_str());
    if (ret < 0)
        std::cout << "Failed to delete " << ib_name << std::endl;

    del_file = "rm -rf /dev/shm/" + string(bi_name);
    ret = system(del_file.c_str());
    if (ret < 0)
        std::cout << "Failed to delete " << bi_name << std::endl;

    delete[] ib_name;
    delete[] bi_name;
}

bool ShmemCommunicator::is_empty() {
    return pb_is_empty(recv_buffer);
}

int ShmemCommunicator::send_packet(Packet * pPacket) {
    while (pb_is_full(send_buffer)){nanosleep(&ts, NULL); };
    pb_write(send_buffer, pPacket);
	return sizeof(pPacket);
}

int ShmemCommunicator::irecv_packet(Packet * pPacket) {
    if (pb_is_empty(recv_buffer))
		return 0;
    
	else
        return pb_read(recv_buffer, pPacket);
}

int ShmemCommunicator::recv_packet(Packet * pPacket) {
    while (pb_is_empty(recv_buffer)) { nanosleep(&ts, NULL); };
    pb_read(recv_buffer, pPacket);
	return sizeof(pPacket);
}

bool ShmemCommunicator::prepare_connection(const char *name, int unique_id) {
    struct passwd *pd = getpwuid(getuid());  // Check for NULL!

    ib_name = new char[64];
	bi_name = new char[64];

	srand((unsigned int) time(0));

    sprintf(ib_name, "/%s_shmem_ib_%d_%d",pd->pw_name, unique_id, 0);
    sprintf(bi_name, "/%s_shmem_bi_%d_%d",pd->pw_name, unique_id, 0);

    stringstream filePath;
    filePath << ROOT_PATH << "shared/.args.shmem.dat_" << unique_id;
    ofstream ofs(filePath.str());
    assert(ofs.is_open());
    ofs << name << endl << ib_name << endl << bi_name << endl;
    
	ofs << "buffer_size " << PKT_BUFFER_SIZE << endl;
    ofs.close();

    send_buffer = (PacketBuffer*)establish_shm_segment(bi_name, sizeof(PacketBuffer));
    memset(send_buffer, 0x0, sizeof(PacketBuffer));
    recv_buffer = (PacketBuffer*)establish_shm_segment(ib_name, sizeof(PacketBuffer));
    memset(recv_buffer, 0x0, sizeof(PacketBuffer));

    pb_init(send_buffer);
    pb_init(recv_buffer);

    if (!send_buffer || !recv_buffer) {
        return false;
    }

    return true;
}

void ShmemCommunicator::wait_connection() {
    cout << "Wait for the reponse from the host... " << endl;
    Packet p;
    recv_packet(&p);
    cout << "Shmem is established" << endl;
}

void *ShmemCommunicator::establish_shm_segment(char *name, int size) {
    int fd;

    fd = shm_open(name, O_RDWR | O_CREAT, 0600);
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
