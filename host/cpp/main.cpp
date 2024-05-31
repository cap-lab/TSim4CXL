#include "traceGenerator.h"

//#define TEST_MODE
//#define TEST_SIZE 1024

void parse_trace(string file, vector<tuple<string, uint32_t, uint32_t, string>>& trace) {
	ifstream input(file);
 	if (input.is_open())
		cout << file << " opened successfully.\n";
	else
		cerr << "Failed to open file: " << file << '\n';

	string line;
    while (getline(input, line)) {
        istringstream iss(line);
        string api, address, size, data;
        getline(iss, api, ':');
        getline(iss, address, ':');
        getline(iss, size, ':');
        getline(iss, data);
        tuple<string, uint32_t, uint32_t, string> tup(api, stoul(address), stoul(size), data);
        trace.push_back(tup);
	}
    input.close();
}

void parse_delta_trace(string d_file, vector<uint64_t>& delta_trace) {
	ifstream input(d_file);
	string line;
    while (getline(input, line)) {
        istringstream iss(line);
        string delta;
        getline(iss, delta);
        delta_trace.push_back(stoul(delta));
	}
    input.close();
}

void str_to_array(string d, double *data) {
	istringstream iss(d);
	vector<double> data_vec(istream_iterator<double>(iss), {});
	copy(data_vec.begin(), data_vec.end(), data);
}

void host(int id) {
    traceGenerator* t = new traceGenerator(id, "host_" + to_string(id));
    vector<tuple<string, uint32_t, uint32_t, string>> trace;
	string trace_f = "../trace/r" + to_string(id) + ".txt";
	string delta_f = "../trace/d" + to_string(id) + ".txt";

    vector<uint64_t> delta_trace;
	parse_trace(trace_f, trace);
	parse_delta_trace(delta_f, delta_trace);
	
	for (size_t i = 0; i < trace.size(); i++) {
		string api, data;
		uint32_t address, size;
		tie(api, address, size, data) = trace[i];
		uint64_t delta = delta_trace[i];
		
		#ifdef TEST_MODE
		size = TEST_SIZE;
		#endif
		
		/* Write */
		if (api == "W") {
			uint8_t* uint_array = new uint8_t[size];
			for (int i = 0; i < (int)size; i++)
				uint_array[i] = i;

			t->trcWrite(address, uint_array, size, 0, delta);
		}
		/* Read */
		else if (api == "R") {
        	uint8_t* data_array = new uint8_t[size];
			
			t->trcRead(address, data_array, size, 0, delta);
			
			#ifdef PRINT	
			double* double_array = reinterpret_cast<double*>(data_array);
			printArray(double_array);
			#endif
		}
		/* Terminate */
		else
			t->trcTerminate(delta);
    }
}

int main(int argc, char* argv[]) {
    int id = stoi(argv[1]);
    host(id);
    return 0;
}
