#include "remote_execution_manager.h"

#include <cstring>

#include <thread>
#include <unistd.h>

#include <stdexcept>
#include <format>

using std::async;
using std::future;

using std::runtime_error;
using std::format;

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
	std::lock_guard<std::recursive_mutex> lock(serializer);

	delayed_dispatches.emplace_back(new Dispatch(nullptr, filename, line));
}

/**
	Dispatches a command to one remote machine in the machine dataset.
	Assumes a dispatch object has been created and a machine is available.

	@param dispatch Dispatch to send to the remote machine
*/
void RemoteExecutionManager::dispatch(Dispatch *dispatch) {
	// Serialize concurrent calls to this method
	std::lock_guard<std::recursive_mutex> lock(serializer);

	remote_dispatches.push_back(dispatch);

	// Launch a separate thread that will run the task remotely, collect the result
	// and fill up the dispatch result with the outcome
	remote_dispatch_results.emplace_back(
		std::move(async(std::launch::async, [this, dispatch] {
			char *command_line1[] = {
				"ssh",
				(char *) dispatch->machine->name.c_str(),
				"<working_directory>/local_runner.sh",
				(char *) dispatch->filename.c_str(),
				nullptr
			};

			run_local(command_line1, &dispatch->pid, &dispatch->exit_value);

			char *command_line2[] = {
				"rm",
				"-f",
				(char *) dispatch->filename.c_str(),
				nullptr
			};

			run_local(command_line2, nullptr, nullptr);

			// Synchronized write to the variable
			dispatch->machine->numberSlots.fetch_add(1);

			return (dispatch->exit_value == 1 ? true : false);
		}))
	);

	remote_dispatch_results.back().wait_for(std::chrono::seconds(0));
}

/**
	Run the specified command locally, collecting the output.
	Individual lines bigger than 1K characters are truncated.

	@param command The command to be run locally under /bin/sh
	@param pid If different than nullptr, fill up with the process PID
	@param exit_value If different than nullptr fill up with the process exit value
*/
void RemoteExecutionManager::run_local(char *const command_line[], pid_t *pid, int *exit_value) {
	FILE *output_stream;
	char output_line[1024];

	int child_pid = fork();

	// If there's an error forking, kill all previous dispatches and exit the program
	if(child_pid == -1) {
		kill_dispatches();

		throw runtime_error("Error forking child process");
	}

	// Child process goes here
	if(child_pid == 0) {
		execvp(command_line[0], command_line);

		// Never reached
		exit(EXIT_SUCCESS);
	}

	// Parent process goes here
	if(pid != nullptr) {
		*pid = child_pid;
	}

	if(exit_value != nullptr) {
		// Waits until the child process terminates
		int status;

		waitpid(child_pid, &status, 0);

		// Collects the exit status
		*exit_value = WEXITSTATUS(status);
	}
}

/**
	Waits until all previous dispatches were successful.
*/
RemoteExecutionManager::ClearingResult RemoteExecutionManager::clear_dispatches() {
	// Serialize concurrent calls to this method
	std::lock_guard<std::recursive_mutex> lock(serializer);

	while(delayed_dispatches.size() != 0) {
		int next_machine = find_machine();

		if(next_machine == -1) {
			break;
		}

		Dispatch *new_dispatch = delayed_dispatches.back();
		delayed_dispatches.pop_back();

		new_dispatch->machine = remote_machines[next_machine];
		dispatch(new_dispatch);
	}

	for(uint i = 0; i < remote_dispatches.size(); i++) {
		if(!remote_dispatches[i]) {
			continue;
		}

		bool success = remote_dispatch_results[i].get();

		// If we just completed one dispatch and we have queued dispatches,
		// schedule them right now
		if(delayed_dispatches.size() != 0) {
			Dispatch *new_dispatch = delayed_dispatches.back();
			delayed_dispatches.pop_back();

			new_dispatch->machine = remote_dispatches[i]->machine;
			dispatch(new_dispatch);
		}

		// Clean the old dispatch information
		delete remote_dispatches[i];

		remote_dispatches[i] = nullptr;

		if(success) {
			return ClearingResult::Sat;
		}
		else {
			return ClearingResult::Unsat;
		}
	}

	return ClearingResult::Done;
}

/**
	Waits until all previous dispatches were successful.
*/
void RemoteExecutionManager::kill_dispatches() {
	// Serialize concurrent calls to this method
	std::lock_guard<std::recursive_mutex> lock(serializer);

	for(uint i = 0; i < remote_dispatches.size(); i++) {
		if(!remote_dispatches[i]) {
			continue;
		}

		string pid_string = std::to_string(remote_dispatches[i]->pid);

		char *command_line[] = { "kill", "-9", (char *) pid_string.c_str(), nullptr };

		run_local(command_line, nullptr, nullptr);
	}
}