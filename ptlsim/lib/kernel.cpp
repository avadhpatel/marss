//
// PTLsim: Cycle Accurate x86-64 Simulator
// Linux Kernel Interface
//
// Copyright 2000-2008 Matt T. Yourst <yourst@yourst.com>
//

#include <globals.h>
#include <superstl.h>
#include <mm.h>
#include <ptlsim.h>
#include <stats.h>
#include <kernel.h>
//#include <loader.h>


/*
asmlinkage void assert_fail(const char *__assertion, const char *__file, unsigned int __line, const char *__function) {
  // use two stringbufs to avoid allocating any memory:
  stringbuf sb1, sb2;
  sb1 << endl, "Assert ", __assertion, " failed in ", __file, ":", __line, " (", __function, ") from ", getcaller();
  sb2 << " at ", sim_cycle, " cycles, ", total_user_insns_committed, " commits, ", iterations, " iterations", endl;

//  if (!lowlevel_init_done) {
//    sys_write(2, sb1, strlen(sb1));
//    sys_write(2, sb2, strlen(sb2));
//    asm("mov %[ra],%%rax; ud2a;" : : [ra] "r" (getcaller()));
//  }

  cerr << sb1, sb2, flush;

  if (ptl_logfile) {
    ptl_logfile << sb1, sb2, flush;
    PTLsimMachine* machine = PTLsimMachine::getcurrent();
    if (machine) machine->dump_state(ptl_logfile);
    ptl_logfile.close();
  }

  // Make sure the ring buffer is flushed too:
  ptl_mm_flush_logging();
  cerr.flush();
  cout.flush();

  // Force crash here:
  asm("ud2a");
  for (;;) { }
}
*/

#include <execinfo.h>

extern "C" void assert_fail(const char *__assertion, const char *__file, unsigned int __line, const char *__function) {
  stringbuf sb;
  sb << "Assert ", __assertion, " failed in ", __file, ":", __line, " (", __function, ")", endl;
  sb << "Printing stack trace:\n";
  // Print backtrace
	void *array[10];
	size_t size;
	char **strings;
	size_t i;

	size = backtrace (array, 10);
	strings = backtrace_symbols (array, size);

	printf ("Obtained %zd stack frames.\n", size);

	for (i = 0; i < size; i++)
		sb << strings[i], endl;
		//printf ("%s\n", strings[i]);

	free (strings);

  cerr << sb;
  cerr.flush();
  cout.flush();
  abort();
}

W64 core_freq_hz = 0;

W64 get_core_freq_hz() {
  if likely (core_freq_hz) return core_freq_hz;

  W64 hz = 0;

  ifstream cpufreqis("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq");
  if (cpufreqis) {
    char s[256];
    cpufreqis >> readline(s, sizeof(s));      
    
    int khz;
    int n = sscanf(s, "%d", &khz);
    
    if (n == 1) {
      hz = (W64)khz * 1000;
      core_freq_hz = hz;
      return hz;
    }
  }
  
  ifstream is("/proc/cpuinfo");
  
  if (!is) {
    cerr << "get_core_freq_hz(): warning: cannot open /proc/cpuinfo. Is this a Linux machine?", endl;
    core_freq_hz = hz;
    return hz;
  }
  
  while (is) {
    char s[256];
    is >> readline(s, sizeof(s));
    
    int mhz;
    int n = sscanf(s, "cpu MHz : %d", &mhz);
    if (n == 1) {
      hz = (W64)mhz * 1000000;
      core_freq_hz = hz;
      return hz;
    }
  }

  // Can't read either of these procfiles: abort
  assert(false);
  return 0;
}

