#ifndef REMOTE_EXECUTION_MANAGER_H
#define REMOTE_EXECUTION_MANAGER_H

#include <vector>
#include <queue>
#include <memory>

#include <string>

#include <atomic>
#include <future>

using std::vector;
using std::queue;
using std::shared_ptr;

using std::string;

using std::atomic_uint;
using std::future;

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

private:
	vector<Machine *> remote_machines;
	vector<Dispatch *> remote_dispatches;
	vector<future<bool>> remote_dispatch_results;

	uint search_offset;

public:
	RemoteExecutionManager();
	virtual ~RemoteExecutionManager();

	template<class T = string>
	void add_machine(T &&machine, uint numberSlots);
	void dispatch(string command, uint line);

	template<class T = string>
	string run_local(T &&command);

	Dispatch *collect_ready_dispatches(WaitMode waitMode = WaitMode::NoWait);
	Dispatch *clear_dispatches();
};

#endif /* REMOTE_EXECUTION_MANAGER_H */