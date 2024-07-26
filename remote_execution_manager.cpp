#include "remote_execution_manager.h"

#include <cstring>

#include <thread>
#include <unistd.h>

using std::async;
using std::future;

/**
	Default constructor: adds a list of default machines into the machine dataset.
*/
RemoteExecutionManager::RemoteExecutionManager() {
	search_offset = 0;

	add_machine(string("localhost"), 1);
	add_machine(string("localhost"), 1);
	add_machine(string("localhost"), 1);
}

/**
	Default destructor: clears the internal data structures. It is the responsibility of the user
	to clear all dispatches before the deletion.
*/
RemoteExecutionManager::~RemoteExecutionManager() {
	for(auto *machine: remote_machines) {
		delete machine;
	}

	for(auto *dispatch: remote_dispatches) {
		if(dispatch) {
			delete dispatch;
		}
	}
}

/**
	Adds a machine to the machine dataset. If a machine is added X times,
	X processes might be simultaneously dispatched to it.

	@param machine Hostname to insert in the machine dataset
	@param numberSlots Number of slots of execution that can be fulfilled with tasks within the machine
*/
void RemoteExecutionManager::add_machine(string machine_name, uint numberSlots) {
	remote_machines.push_back(new Machine(machine_name, numberSlots));
}

/**
    Returns the index of an available machine.
 
	@return The index of an available machine or -1 if none are available.
 */
int RemoteExecutionManager::find_machine() {
	// Find a machine to execute
	int next_machine = -1;

	for(uint i = 0; i < remote_machines.size(); i++) {
		uint j = (i + search_offset) % remote_machines.size();

		// Note that this only works because we are assuming that there is a single
		// dispatcher thread
		if(remote_machines[j]->numberSlots > 0) {
			remote_machines[j]->numberSlots.fetch_sub(1);

			next_machine = j;
			break;
		}
	}

	if(next_machine != -1) {
		// Next time, start from a different entry
		search_offset++;
	}

	return next_machine;
}

/**
	Dispatches a command to one remote machine in the machine dataset.
	If no machine is available, queue the dispatch.

	@param command Command to execute in the remote machine
	@param line Line in the VIPR file to which the execution is related
*/
void RemoteExecutionManager::dispatch(string filename, uint line) {
	// Serialize concurrent calls to this method
	std::lock_guard<std::mutex> lock(serializer);

	// Find a machine to execute
	int next_machine = find_machine();

	if(next_machine == -1) {
		delayed_dispatches.emplace_back(new Dispatch(nullptr, filename, line));
		return;
	}

	// Create the dispatch to the available machine
	Dispatch *next_dispatch = new Dispatch(remote_machines[next_machine], filename, line);

	dispatch(next_dispatch);
}

/**
	Dispatches a command to one remote machine in the machine dataset.
	Assumes a dispatch object has been created and a machine is available.

	@param dispatch Dispatch to send to the remote machine
*/
void RemoteExecutionManager::dispatch(Dispatch *dispatch) {
	remote_dispatches.push_back(dispatch);

	// Launch a separate thread that will run the task remotely, collect the result
	// and fill up the dispatch result with the outcome
	remote_dispatch_results.emplace_back(
		std::move(async(std::launch::async, [this, dispatch] {
			string ssh_command = "ssh " + dispatch->machine->name + " <working_directory>/local_runner.sh " + dispatch->filename;

			string output = run_local(ssh_command);

			run_local(string("rm -f " + dispatch->filename));

			// Synchronized write to the variable
			dispatch->machine->numberSlots.fetch_add(1);

			if(output != "sat\n") {
				return false;
			}

			return true;
		}))
	);

	remote_dispatch_results.back().wait_for(std::chrono::seconds(0));
}

/**
	Run the specified command locally, collecting the output.
	Individual lines bigger than 1K characters are truncated.

	@param command The command to be run locally under /bin/sh

	@return The output of the command.
*/
string RemoteExecutionManager::run_local(string command) {
	FILE *output_stream;
	char output_line[1024];

	string output_contents;

	if((output_stream = popen(command.c_str(), "r")) != NULL) {
		while(fgets(output_line, sizeof(output_line), output_stream) != NULL) {
			output_contents.append(std::string(output_line));
		}
	}

	pclose(output_stream);

	return output_contents;
}

/**
	Waits until all previous dispatches were successful.
*/
RemoteExecutionManager::ClearingResult RemoteExecutionManager::clear_dispatches() {
	// Serialize concurrent calls to this method
	std::lock_guard<std::mutex> lock(serializer);

	while(true) {
		bool restart = false;

		for(uint i = 0; i < remote_dispatches.size(); i++) {
			if(!remote_dispatches[i]) {
				continue;
			}

			bool success = remote_dispatch_results[i].get();

			// If we just completed one dispatch and we have queued dispatches,
			// schedule them right now
			if(delayed_dispatches.size() != 0) {
				Machine *old_machine = remote_dispatches[i]->machine;

				Dispatch *new_dispatch = delayed_dispatches.back();
				delayed_dispatches.pop_back();

				new_dispatch->machine = old_machine;
				dispatch(new_dispatch);

				// UGH, but I'm in a hurry
				restart = true;
				break;
			}

			// Clean the old dispatch information
			remote_dispatches[i] = nullptr;
			delete remote_dispatches[i];

			if(success) {
				return ClearingResult::Sat;
			}
			else {
				return ClearingResult::Unsat;
			}
		}

		if(!restart) {
			break;
		}
	}

	return ClearingResult::Done;
}