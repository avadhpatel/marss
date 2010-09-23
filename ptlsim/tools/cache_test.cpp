
//
// Copyright 2009 Avadh Patel <apatel@cs.binghamton.edu>
//
// Authors:
//	Avadh Patel
//	Furat Afram
//

#include <memoryRequest.h>
#include <memoryHierarchy.h>
#include <test.h>

using namespace Memory;
// Dummy OutOfOrderMachine struct
namespace OutOfOrderModel {
	struct OutOfOrderMachine
	{
	};
};

void MemoryHierarchy::icache_wakeup_wrapper(int coreid, W64 physaddr)
{
	memdebug("ICache access completed for core: ", coreid,
			" address: ", hexstring(physaddr, 48), endl);
}

void MemoryHierarchy::dcache_wakeup_wrapper(int coreid, int threadid,
		int robid, W64 seq, W64 physaddr)
{
	memdebug("DCache access completed for core: ", coreid,
			" thread: ", threadid, " rob: ", robid,
			" address: ", hexstring(physaddr, 48), endl);
}

Config config;

W64 sim_cycle;

ostream ptl_logfile;

bool test_signal_cb(void *arg)
{
	ptl_logfile << "Test signal fired with arg: ", *((int*)arg), endl, flush;
	return true;
}

void test_event_add(MemoryHierarchy* memoryHierarchy)
{
	cout << "Testing MemoryHierarchy::add_event ";

	stringbuf *sig_name = new stringbuf();
	*sig_name << "Sig1";
	Signal *sig1 = new Signal(sig_name->buf);
	sig1->connect(signal_fun_ptr(test_signal_cb));

	int *arg;
	arg = (int*)(malloc(sizeof(int)*3));
	arg[0] = 0;
	arg[1] = 1;
	arg[2] = 2;

	ptl_logfile << "Before adding events to queue", endl, flush;

	for(int i=0; i < 100; i++) {
		memoryHierarchy->add_event(sig1, i, (void*)(&arg[i % 3]));
	}

	ptl_logfile << "Added events into queue, now printing", endl, flush;
	memoryHierarchy->dump_info(ptl_logfile);
	ptl_logfile << flush;

	ptl_logfile << "Reseting the event queue";
	memoryHierarchy->reset();

	memoryHierarchy->add_event(sig1, 11, (void*)(&arg[0]));
	memoryHierarchy->add_event(sig1, 10, (void*)(&arg[0]));
	memoryHierarchy->add_event(sig1, 8, (void*)(&arg[1]));
	memoryHierarchy->dump_info(ptl_logfile);

	cout << "..Done" << endl;
}

void test_clock(MemoryHierarchy* memoryHierarchy)
{
	cout << "Testing MemoryHierarchy::clock ";

	const char* sig_name2 = "Sig2";
	Signal *sig2 = new Signal(sig_name2);
	sig2->connect(signal_fun_ptr(test_signal_cb));

	int *arg;
	arg = (int*)(malloc(sizeof(int)*2));
	arg[0] = 0;
	arg[1] = 1;

	memoryHierarchy->add_event(sig2, 10, (void*)(&arg[1]));
	for(sim_cycle=0; sim_cycle < 1000; sim_cycle++) {
		if(sim_cycle == 3)
			memoryHierarchy->add_event(sig2, 0, (void*)(&arg[0]));
		if(sim_cycle == 5)
			memoryHierarchy->add_event(sig2, 2, (void*)(&arg[1]));
		memoryHierarchy->clock();
		ptl_logfile << "sim_cycle - ", sim_cycle, endl, flush;
	}

	cout << "..Done" << endl;
}

void test_access_fast_path(MemoryHierarchy *memoryHierarchy)
{
	bool ret_val;

	cout << "Testing Memory Access Fast Path";

	ret_val = memoryHierarchy->access_cache(0, 0, 0, 0, 0, 0x10, true, false);

	ptl_logfile << "access ret value ", ret_val, endl;

	for(sim_cycle; sim_cycle < 1500; sim_cycle++) {
		ptl_logfile << "Clock: ", sim_cycle, endl;
		if(sim_cycle == 3) {
			ret_val = memoryHierarchy->access_cache(0, 0, 0, 0, 0,
					0x86, true, false);

			ptl_logfile << "access ret value ", ret_val, endl;
		}
		memoryHierarchy->clock();
		memoryHierarchy->dump_info(ptl_logfile);
	}

	cout << "..Done" << endl;
}

void test_request_pool()
{
	cout << "Testing request pool.." ;
	RequestPool *requestPool = new RequestPool();

	// First check if we get 100 free requests
	// checked if we can have more than 100 , and it worked with 1500
	MemoryRequest* memoryRequest = NULL;
	foreach(i, 1500) {
		memoryRequest = requestPool->get_free_request();
		assert(memoryRequest != NULL);
	}

	cout << "Done" << endl;
}

struct TestQueueLink {
	int idx;

	void reset(int i) { idx = i; }
	void init() { }
	void print(ostream& os) {
		os << "idx[", idx, "] ";
	}

	bool operator <(TestQueueLink& t) {
		return (this->idx < t.idx);
	}

	bool operator ==(TestQueueLink& t) {
		return (this->idx == t.idx);
	}
};

void test_fix_queuelink()
{
	FixQueueLink<TestQueueLink, 10> testQueue;

	cout << "Testing FixQueueLink...";
	TestQueueLink* entryArr[10];

	ptl_logfile << "Before allocation: ";
	testQueue.print_all(ptl_logfile);
	ptl_logfile << endl;
	foreach(i, 10) {
		TestQueueLink* entry = testQueue.alloc();
		entryArr[i] = entry;
	}
	ptl_logfile << "After allocation: ", testQueue, endl;

	foreach(j, 6) {
		testQueue.free(entryArr[j+2]);
	}
	ptl_logfile << "After freeing 4 entries: ", testQueue, endl;
	ptl_logfile << "All entries: ";
	testQueue.print_all(ptl_logfile);
	ptl_logfile << endl;

	// Now unlink the entry from tail and add to head + 1
	ptl_logfile << "Before unlinking tail entry: ", testQueue, endl;
	TestQueueLink* et1 = testQueue.tail->data;
	testQueue.unlink(et1);
	ptl_logfile << "After unlinking tail entry: ", testQueue, endl;
	testQueue.insert_before(et1, testQueue.head->data);
	ptl_logfile << "After insert tail entry: ", testQueue, endl;

	cout << "Done", endl;
}

struct FixStateListTester : public FixStateListObject
{
	void init() {
	}
	ostream& print(ostream& os) const{
		os << "A FixStateListTester Object[", idx, "]";
		return os;
	}
};

void test_fix_statelist()
{
	FixStateList<FixStateListTester, 10> testList;
	FixStateListTester* entryArr[10];

	cout << "Testing FixStateList\n";
	ptl_logfile << "Testing FixStateList\n";

	ptl_logfile << "Before Allocation: ";
	testList.print_all(ptl_logfile);
	ptl_logfile << endl;
	foreach(i, 10) {
		FixStateListTester* entry = testList.alloc();
		entryArr[i] = entry;
	}
	ptl_logfile << "After allocation: ", testList, endl;

	foreach(j, 6) {
		testList.free(entryArr[j+2]);
	}

	ptl_logfile << "After freeing 6 entries: ", testList, endl;
	ptl_logfile << "All entries: ";
	testList.print_all(ptl_logfile);
	ptl_logfile << endl, flush;

	// Now unlink entry from tail and add to head
	ptl_logfile << "Before unlinking the tail entry: ", testList, endl,
			flush;
	FixStateListTester* tail = testList.tail();
	assert(tail);
	testList.unlink(tail);
	ptl_logfile << "After unlink: ", testList, endl, flush;
	FixStateListTester* head = testList.head();
	assert(head);
	testList.insert_after(tail, head);
	ptl_logfile << "After inserting : ", testList, endl;

	cout << "Done..\n";
	cout << "Done..\n";
}

void test_trace(MemoryHierarchy *memoryHierarchy, char *filename)
{
	istream file;
	file.open(filename);
	stringbuf line;
	dynarray<stringbuf*> splits;
	W64 clock;
	W8 coreid;
	char dataFlag;
	char readFlag;
	W64 addr;

	sim_cycle = 0;

	for(;;) {

		line.reset();
		file >> line;
		if(!file) break;

//		cout << "Line read: ", line, endl, flush;
		line = line.strip();

		if(line.size() == 0 || line.buf[0] == '#')
			continue;

		const char *splitters = " \t\n";
		line.split(splits, splitters);

		foreach(i, splits.count()) {
			if(i == 0) {
				// Clock
//				cout << "Buffer for clock: ", splits[i]->buf, " ";
				sscanf(splits[i]->buf, "%llu", &clock);

				// Loop untill sim_cycle reaches to clock
				for(sim_cycle; sim_cycle < clock; sim_cycle++) {
					cout.seek(0);
					cout << "Execuing cycle: ", sim_cycle, " \r";
					ptl_logfile << "Executing cycle: ", sim_cycle, "\n";
					memoryHierarchy->clock();
				}

			}
			else {
				sscanf(splits[i]->buf, "%hu:%c:%c:0x%llx",
						(short unsigned int*)&coreid,
						&dataFlag, &readFlag, &addr);
//				cout.seek(0);
//				cout << "Coreid: ", coreid, " DataFlag: ", dataFlag,
//					 " Read/Write: ", readFlag,
//					 " Address: 0x", hexstring(addr, 48), " \r";

				memoryHierarchy->access_cache(coreid, 0, 0,
						0, sim_cycle, addr,
						(dataFlag == 'd') ? false : true,
						(readFlag == 'r') ? false : true);
			}

			delete splits[i];
		}
		splits.clear();

	}
	cout << "\nDone..\n";
}

void test_strip()
{
	stringbuf testStr;
	testStr << " \t#\t  Test string with a:0:0x2020 \t \n";
	cout << "String before strip: ", testStr;
	cout << "length before: ", strlen(testStr.buf), endl;
	testStr = testStr.strip();
	cout << "String after strip: ", testStr, endl;
	cout << "length after: ", strlen(testStr.buf), endl;

	dynarray<stringbuf*> strings;
	const char *chr = " \t\n";
	testStr.split(strings, chr);

	foreach(i, strings.count()) {
		cout << "Split[", i, "]: ", *strings[i], endl;
	}
}

int main(int argc, char *argv[])
{

	ptl_logfile.open("mem_test.log");
	ptl_logfile << "Starting mem test log\n", flush;

	ptl_logfile << "NUMBER_OF_CORES = ", NUMBER_OF_CORES, endl, flush;

	config.number_of_cores = 4;
	config.cores_per_L2 = 4;

	ptl_logfile << "config.number_of_cores ", config.number_of_cores, endl, flush;

	sim_cycle = 0;
	OutOfOrderMachine machine;
	MemoryHierarchy *memory = new MemoryHierarchy(machine);

	memory->print_map(ptl_logfile);

	if(argc == 2) {
		test_trace(memory, argv[1]);
		memory->dump_info(ptl_logfile);
		return 0;
	}

	test_event_add(memory);

	test_clock(memory);

	test_request_pool();

	test_fix_queuelink();

	test_fix_statelist();

	test_access_fast_path(memory);

	test_strip();

	memory->dump_info(ptl_logfile);

	ptl_logfile.close();

	cout << "Memory Testing done..." << endl;

	return 0;
}
