#include <components/Top.h>
#include <components/configurations.h>

Configurations cfgs;

int sc_main(int argc, char* argv[])
{
    cfgs.init_configurations();

    Top top("top");
   	
	for (int i = 0; i < cfgs.get_host_num(); i++) {	
		char c[20];
		sprintf(c, "clk%d", i);
		sc_clock* clk = new sc_clock(c, 1/cfgs.get_freq(i), SC_NS);
		top.clock[i].bind(*clk);
	}

	clock_t sim_start = clock();
    sc_start();
    top.finish();
    clock_t time = clock();
	
	cout << "---------------------------------------" << endl;
    cout << "Simulation Time : " << std::setprecision(5) << (double) (time - sim_start) / CLOCKS_PER_SEC << " (seconds)" << endl;
	cout << "Simulated Time : " << (uint64_t) (sc_time_stamp().to_double() / 1000) << " (ns)" << endl;

    return 0;
}
