#ifndef SIM_SYNC_OBJECT_H
#define SIM_SYNC_OBJECT_H

using namespace sc_core;

class SyncObject
{
public:
    SyncObject() {
        for (uint32_t i = 0; i < 2000000000; i++) {
            sync[i] = false;
        }
    }
    ~SyncObject() {}

public:
    void signal(uint32_t sync_id) {
        sync[sync_id] = true;
    }
    
    void free(uint32_t sync_id){
        sync[sync_id] = false;
    } 

    bool check_signal(uint32_t sync_id) {
        if (sync[sync_id])
            return true;
        else
            return false;
    }

private:
    bool sync[2000000000];
};

#endif
