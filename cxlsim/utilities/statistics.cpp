#include "statistics.h"

Statistics::Statistics()
{
    stats.num_read_request = 0;
    stats.num_read_flit = 0;
    stats.num_read_packet = 0;
    stats.total_read_latency = 0;
    stats.total_read_size = 0;
    stats.num_write_request = 0;
    stats.num_write_flit = 0;
    stats.num_write_packet = 0;
    stats.total_write_latency = 0;
    stats.total_write_size = 0;
}

void Statistics::print_stats()
{
	cout << "##### [" << name << "] #####" << '\n';
    
	if (stats.num_read_packet) {
        cout << "<-- READ -->" << '\n';
        cout << "packet num      : " << stats.num_read_packet << '\n';
    	cout << "flit num        : " << stats.num_read_flit << '\n';
        cout << "total size      : " << stats.total_read_size << '\n';
    }
    
	if (stats.num_write_packet) {
        cout << "<-- WRITE -->" << '\n';
        cout << "packet num      : " << stats.num_write_packet << '\n';
    	cout << "flit num        : " << stats.num_write_flit << '\n';
        cout << "total size      : " << stats.total_write_size << '\n';
    }
	cout << "------------------------------------" << '\n';
}

void Statistics::print_mem_stats()
{
    unsigned average_read_latency = 0;
    unsigned average_read_size = 0;
    unsigned average_write_latency = 0;
    unsigned average_write_size = 0;
	
	cout << "##### [" << name << "] #####" << '\n';
    
	if (stats.num_read_request > 0) {
        average_read_latency = stats.total_read_latency / stats.num_read_request;
        average_read_size = stats.total_read_size / stats.num_read_request;

    }
    
	if (stats.num_write_request > 0) {
        average_write_latency = stats.total_write_latency / stats.num_write_request;
        average_write_size = stats.total_write_size / stats.num_write_request;
    }
    
	cout << "<-- READ -->" << '\n';
    cout << "request num     : " << stats.num_read_request << '\n';
    cout << "total size      : " << stats.total_read_size << '\n';
    cout << "average size    : " << average_read_size << '\n';
	cout << "total latency   : " << stats.total_read_latency << " (ns)" << '\n';
    cout << "average latency : " << average_read_latency << " (ns)" << '\n';
    
	cout << "<-- WRITE -->" << '\n';
    cout << "request num     : " << stats.num_write_request << '\n';
    cout << "total size      : " << stats.total_write_size << '\n';
    cout << "average size    : " << average_write_size << '\n';
	cout << "total latency   : " << stats.total_write_latency << " (ns)" << '\n';
    cout << "average latency : " << average_write_latency << " (ns)" << '\n';

	cout << "------------------------------------" << '\n';
}
