
//
// Copyright 2009 Avadh Patel <apatel@cs.binghamton.edu>
//
// Authors:
//	Avadh Patel
//	Furat Afram
//

#ifndef MEM_TEST_H
#define MEM_TEST_H

#include <globals.h>
#include <superstl.h>

struct Config {
	int number_of_cores;
	int cores_per_L2;
	int loglevel;
} ;

extern Config config;

extern W64 sim_cycle;

extern ostream ptl_logfile;

#endif // MEM_TEST_H
