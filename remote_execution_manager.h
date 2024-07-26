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

using std::recursive_mutex;

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
		pid_t pid;
		int exit_value;
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

	recursive_mutex serializer;

public:
	RemoteExecutionManager();
	virtual ~RemoteExecutionManager();

	void dispatch(string filename, uint line);

	ClearingResult clear_dispatches();
	void kill_dispatches();

private:
	void add_machine(string machine_name, uint numberSlots);
	int find_machine();

	void dispatch(Dispatch *dispatch);
	void run_local(char *const command_line[], pid_t *pid, int *exit_value);
};

#endif /* REMOTE_EXECUTION_MANAGER_H */