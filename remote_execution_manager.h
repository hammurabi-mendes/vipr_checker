#ifndef REMOTE_EXECUTION_MANAGER_H
#define REMOTE_EXECUTION_MANAGER_H

#include <vector>
#include <queue>
#include <memory>

#include <string>

#include <atomic>
#include <future>

#include <mutex>

using std::vector;
using std::queue;
using std::shared_ptr;

using std::string;

using std::atomic_uint;
using std::future;

using std::mutex;

class RemoteExecutionManager {
public:
	struct Machine {
		Machine(string &name, uint numberSlots): name(name), numberSlots(numberSlots) {};

		string name;
		atomic_uint numberSlots;
	};

	struct Dispatch {
		Dispatch(Machine *machine, string &filename, uint line): machine(machine), filename(filename), line(line) {};

		Machine *machine;
		string filename;
		uint line;
	};

	enum WaitMode {
		NoWait,
		WaitFirst,
		WaitFirstFaulty
	};

	enum ClearingResult {
		Sat,
		Unsat,
		Done
	};

private:
	vector<Machine *> remote_machines;
	vector<Dispatch *> remote_dispatches;
	vector<Dispatch *> delayed_dispatches;
	vector<future<bool>> remote_dispatch_results;

	uint search_offset;

	mutex serializer;

public:
	RemoteExecutionManager();
	virtual ~RemoteExecutionManager();

	void add_machine(string machine_name, uint numberSlots);
	int find_machine();

	void dispatch(string filename, uint line);
	void dispatch(Dispatch *dispatch);

	string run_local(string command);

	ClearingResult clear_dispatches();
};

#endif /* REMOTE_EXECUTION_MANAGER_H */