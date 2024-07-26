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
	Dispatches a command to one remote machine in the machine dataset.

	@param command Command to execute in the remote machine
	@param line Line in the VIPR file to which the execution is related
*/
void RemoteExecutionManager::dispatch(string old_filename, uint line) {
	// Serialize concurrent calls to this method
	std::lock_guard<std::mutex> lock(serializer);

	// Find a machine to execute
	int next_machine = -1;

	while(next_machine == -1) {
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

		if(next_machine == -1) {
			collect_ready_dispatches(WaitFirst);
		}
	}

	// Next time, start from a different entry
	search_offset++;

	// Generate a unique filename for the execution based on this template
	char new_filename_template[FILENAME_MAX];
	strcpy(new_filename_template, "SMT_XXXXXX.smt");

	// We need to generate files using mkstemps in order to avoid an unlikely (but possible) race condition in the filesystem
	// as we create files with random names
	int new_filename_fd = mkstemps(new_filename_template, 4);
	close(new_filename_fd);

	// At this point, new_filename_template has been modified and contains a unique filename
	string new_filename = std::string(new_filename_template);

	// Rename the old filename to the new filename
	run_local(string("mv " + old_filename + " " + new_filename));

	// Create the dispatch to the available machine
	Dispatch *dispatch = new Dispatch(remote_machines[next_machine], new_filename, line);
	remote_dispatches.push_back(dispatch);

	// Launch a separate thread that will run the task remotely, collect the result
	// and fill up the dispatch result with the outcome
	remote_dispatch_results.emplace_back(
		std::move(async(std::launch::async, [this, dispatch] {
			string ssh_command = "ssh " + dispatch->machine->name + " <working directory>/local_runner.sh " + dispatch->filename;

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
	Collects all dispatches that are ready.

	@param wait Specifies the waiting mode of the function:
		1) If WaitMode::NoWait, returns nullptr if no dispatch is ready.
		2) If WaitMode::WaitFirst, returns nullptr if the outcome of the collected dispatch was successful,
			or a pointer to the (faulty) collected dispatch otherwise.
		3) If WaitMode::WaitFirstFaulty, returns nullptr only if all dispatches have been collected or if
			a faulty dispatch is collected.

	@return A pointer to a faulty dispatch, if one is collected; or a null pointer otherwise.
*/
RemoteExecutionManager::Dispatch *RemoteExecutionManager::collect_ready_dispatches(RemoteExecutionManager::WaitMode waitMode) {
	// Serialize concurrent calls to this method
	std::lock_guard<std::mutex> lock(serializer);

	while(true) {
		bool foundDispatches = false;

		for(uint i = 0; i < remote_dispatches.size(); i++) {
			if(!remote_dispatches[i]) {
				continue;
			}

			foundDispatches = true;

			if (remote_dispatch_results[i].wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
				bool success = remote_dispatch_results[i].get();

				Dispatch *remote_dispatch = remote_dispatches[i];
				remote_dispatches[i] = nullptr;

				if(!success) {
					return remote_dispatch;
				}
				else {
					delete remote_dispatch;

					if(waitMode == WaitFirst) {
						return nullptr;
					}
				}
			}
		}

		if(waitMode == NoWait || !foundDispatches) {
			break;
		}

		std::this_thread::sleep_for(std::chrono::seconds(1));
	}

	return nullptr;
}

/**
	Waits until all previous dispatches were successful.
*/
RemoteExecutionManager::Dispatch *RemoteExecutionManager::clear_dispatches() {
	// Serialize concurrent calls to this method
	std::lock_guard<std::mutex> lock(serializer);

	for(uint i = 0; i < remote_dispatches.size(); i++) {
		if(!remote_dispatches[i]) {
			continue;
		}

		bool success = remote_dispatch_results[i].get();

		if(!success) {
			return remote_dispatches[i];
		}
		else {
			delete remote_dispatches[i];

			remote_dispatches[i] = nullptr;
		}
	}

	return nullptr;
}