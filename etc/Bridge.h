#include <list>
#include <string>
#include "tlm_utils/simple_initiator_socket.h"

#include "base/base.h"
#include "base/request.h"
#include "base/config.h"
#include "frontend/frontend.h"
#include "memory_system/memory_system.h"

using namespace tlm;
using namespace std;

namespace ramulator2
{
class Bridge {
public:
    struct Queue {
        list<tlm_generic_payload *> q;
        unsigned int size() {return q.size();}
    };

    Bridge(const char *config_path, int id);
    ~Bridge();

    void ClockTick();
    void sendCommand(tlm_generic_payload& trans);
    tlm_generic_payload* getCompletedCommand(void);

private:
	bool enqueue_success;
	string config_path;
	Ramulator::IFrontEnd* frontend;
	Ramulator::IMemorySystem* memory_system;

    Queue in_queue;
    Queue out_queue;

    void *m_memory;
};

}
