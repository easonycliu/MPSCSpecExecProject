/*
 * Copyright (C) 2023 The Osprey Authors
 * All rights reserved.
 *
 * This file is part of Osprey.
 *
 * Osprey is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * Osprey is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with Osprey. If not, see <https://www.gnu.org/licenses/>.
 */

#include <fcntl.h>
#include <poll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <boost/program_options.hpp>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "lib/environment.hpp"
#include "lib/page_allocator.hpp"
#include "memprog/handler.hpp"
#include "memprog/programmer.hpp"
#include "memprog/tracer.hpp"
#include "memprog/watcher.hpp"
#include "util/overlay.hpp"

namespace po = boost::program_options;

struct OspreyConfig {
	const char* osprey_name;

	bool speculative_only;
	std::string tracing_algorithm;
	std::size_t window_size;
	std::string trace_filebase;
	bool trace_to_stdout;

	bool programmed_only;
	bool program_from_parent;
	std::string programming_algorithm;
	std::size_t mem_limit_low;
	std::size_t mem_limit_high;
	std::size_t mem_limit_max;
	std::size_t batch_size;

	bool no_overlay;
	bool preserve_overlay_directories;
	bool cleanup_overlays;

	int program_argc;
	char** program_argv;
};

struct SpeculativeProcessInfo {
	pid_t child_pid;
	int child_comm_fd;
	std::thread backing_thread;
};

struct ProgrammedProcessInfo {
	pid_t child_pid;
	int child_fd;
	int child_comm_fd;
	std::thread backing_thread;
};

void print_usage(const char* osprey_name, bool show_help_hint = true) {
	std::cout << "Usage: " << osprey_name << " [options] [target program invocation and arguments]" << std::endl;
	if (show_help_hint) {
		std::cout << "Hint: use the '--help' option for a description of the available options" << std::endl;
	}
}

void launch_speculative_process(
	SpeculativeProcessInfo& speculative_info, OspreyConfig& config, const osprey::util::Overlay* speculative_overlay
) {
	/* For parent-child communication. */
	int comm_fds[2];
	if (socketpair(AF_UNIX, SOCK_SEQPACKET, 0, comm_fds) != 0) {
		std::perror("socketpair");
		std::exit(EXIT_FAILURE);
	}

	pid_t child_pid = fork();
	if (child_pid == 0) {
		/* Set speculative mode for the spawned process. */
		if (setenv("OSPREY_MODE", "SPECULATIVE", 1) != 0) {
			std::perror("setenv");
			std::exit(EXIT_FAILURE);
		}

		if (setenv("OSPREY_TRACE_FILEBASE", config.trace_filebase.c_str(), 1) != 0) {
			std::perror("setenv");
			std::exit(EXIT_FAILURE);
		}

		if (!config.speculative_only) {
			if (setenv("OSPREY_USE_SHM", "", 1) != 0) {
				std::perror("setenv");
				std::exit(EXIT_FAILURE);
			}
		}

		std::string window_size_string = std::to_string(config.window_size);
		if (setenv("OSPREY_WINDOW_SIZE", window_size_string.c_str(), 1) != 0) {
			std::perror("setenv");
			std::exit(EXIT_FAILURE);
		}

		if (setenv("OSPREY_TRACING_ALGORITHM", config.tracing_algorithm.c_str(), 1) != 0) {
			std::perror("setenv");
			std::exit(EXIT_FAILURE);
		}

		/* Set up communication with parent. */
		if (fcntl(osprey::lib::parent_comm_fd, F_GETFD) != -1 || errno != EBADF) {
			std::cerr << "Fatal error: existing file descriptor conflicts with osprey::lib::parent_comm_fd"
					  << std::endl;
			std::exit(EXIT_FAILURE);
		}
		if (dup2(comm_fds[1], osprey::lib::parent_comm_fd) == -1) {
			std::perror("dup2");
			std::exit(EXIT_FAILURE);
		}
		if (close(comm_fds[0]) != 0) {
			std::perror("close");
			std::exit(EXIT_FAILURE);
		}
		if (close(comm_fds[1]) != 0) {
			std::perror("close");
			std::exit(EXIT_FAILURE);
		}

		/* Mount overlay. */
		if (!config.no_overlay) {
			if (speculative_overlay == nullptr) {
				std::cerr << "Fatal bug: overlay enabled but not initialized" << std::endl;
				std::exit(EXIT_FAILURE);
			}
			speculative_overlay->use_overlay();
		}

		int rv = execvp(config.program_argv[0], config.program_argv);
		(void) rv;
		std::cout << "Could not start target program \"" << config.program_argv[0] << "\": " << std::strerror(errno)
				  << std::endl;
		std::exit(EXIT_FAILURE);
	} else if (child_pid > 0) {
		speculative_info.child_pid = child_pid;
		speculative_info.child_comm_fd = comm_fds[0];

		/*
		 * Ensure that any file descriptors we've opened aren't unwittingly
		 * passed to another process.
		 */
		if (fcntl(speculative_info.child_comm_fd, F_SETFD, FD_CLOEXEC) != 0) {
			std::perror("fcntl(speculative_info.child_comm_fd, F_SETFD, FD_CLOEXEC)");
			std::exit(EXIT_FAILURE);
		}
	} else {
		std::perror("fork");
		std::exit(EXIT_FAILURE);
	}
}

int speculative_only(OspreyConfig& config) {
	SpeculativeProcessInfo speculative_info;

	/* Setup overlay for speculative process. */
	std::unique_ptr<osprey::util::Overlay> speculative_overlay;
	if (!config.no_overlay) {
		speculative_overlay = osprey::util::Overlay::create_overlay();
	}

	launch_speculative_process(speculative_info, config, speculative_overlay.get());

	/* Now, reap the child. */
	{
		int child_status;
		if (waitpid(speculative_info.child_pid, &child_status, 0) != speculative_info.child_pid) {
			std::perror("waitpid");
			return EXIT_FAILURE;
		}
	}

	/* Cleanup overlay. */
	if (!config.no_overlay) {
		speculative_overlay->cleanup_overlay(config.preserve_overlay_directories);
	}

	return EXIT_SUCCESS;
}

void launch_programmed_process(ProgrammedProcessInfo& programmed_info, OspreyConfig& config) {
	/* For parent-child communication. */
	int comm_fds[2];
	if (socketpair(AF_UNIX, SOCK_SEQPACKET, 0, comm_fds) != 0) {
		std::perror("socketpair");
		std::exit(EXIT_FAILURE);
	}

	pid_t child_pid = fork();
	if (child_pid == 0) {
		/* Set programmed mode for the spawned process. */
		if (setenv("OSPREY_MODE", "PROGRAMMED", 1) != 0) {
			std::perror("setenv");
			std::exit(EXIT_FAILURE);
		}

		if (setenv("OSPREY_TRACE_FILEBASE", config.trace_filebase.c_str(), 1) != 0) {
			std::perror("setenv");
			std::exit(EXIT_FAILURE);
		}

		if (!config.programmed_only) {
			if (setenv("OSPREY_USE_SHM", "", 1) != 0) {
				std::perror("setenv");
				std::exit(EXIT_FAILURE);
			}
		}

		std::string mem_limit_low_string = std::to_string(config.mem_limit_low);
		if (setenv("OSPREY_MEM_LIMIT_LOW", mem_limit_low_string.c_str(), 1) != 0) {
			std::perror("setenv");
			std::exit(EXIT_FAILURE);
		}

		std::string mem_limit_high_string = std::to_string(config.mem_limit_high);
		if (setenv("OSPREY_MEM_LIMIT_HIGH", mem_limit_high_string.c_str(), 1) != 0) {
			std::perror("setenv");
			std::exit(EXIT_FAILURE);
		}

		std::string mem_limit_max_string = std::to_string(config.mem_limit_max);
		if (setenv("OSPREY_MEM_LIMIT_MAX", mem_limit_max_string.c_str(), 1) != 0) {
			std::perror("setenv");
			std::exit(EXIT_FAILURE);
		}

		std::string batch_size_string = std::to_string(config.batch_size);
		if (setenv("OSPREY_BATCH_SIZE", batch_size_string.c_str(), 1) != 0) {
			std::perror("setenv");
			std::exit(EXIT_FAILURE);
		}

		if (config.program_from_parent) {
			if (setenv("OSPREY_PROGRAMMING_ALGORITHM", "PARENT", 1) != 0) {
				std::perror("setenv");
				std::exit(EXIT_FAILURE);
			}
		} else {
			if (setenv("OSPREY_PROGRAMMING_ALGORITHM", config.programming_algorithm.c_str(), 1) != 0) {
				std::perror("setenv");
				std::exit(EXIT_FAILURE);
			}
		}

		/* Set up communication with parent. */
		if (fcntl(osprey::lib::parent_comm_fd, F_GETFD) != -1 || errno != EBADF) {
			std::cerr << "Fatal error: existing file descriptor conflicts with osprey::lib::parent_comm_fd"
					  << std::endl;
			std::exit(EXIT_FAILURE);
		}
		if (dup2(comm_fds[1], osprey::lib::parent_comm_fd) == -1) {
			std::perror("dup2");
			std::exit(EXIT_FAILURE);
		}
		if (close(comm_fds[0]) != 0) {
			std::perror("close");
			std::exit(EXIT_FAILURE);
		}
		if (close(comm_fds[1]) != 0) {
			std::perror("close");
			std::exit(EXIT_FAILURE);
		}

		int rv = execvp(config.program_argv[0], config.program_argv);
		(void) rv;
		std::cout << "Could not start target program \"" << config.program_argv[0] << "\": " << std::strerror(errno)
				  << std::endl;
		std::exit(EXIT_FAILURE);
	} else if (child_pid > 0) {
		if (close(comm_fds[1]) != 0) {
			std::perror("close");
			std::exit(EXIT_FAILURE);
		}

		int child_fd = syscall(SYS_pidfd_open, child_pid, 0);
		if (child_fd == -1) {
			std::perror("pidfd_open");
			std::exit(EXIT_FAILURE);
		}

		programmed_info.child_pid = child_pid;
		programmed_info.child_fd = child_fd;
		programmed_info.child_comm_fd = comm_fds[0];

		/*
		 * Ensure that any file descriptors we've opened aren't unwittingly
		 * passed to another process.
		 */
		if (fcntl(programmed_info.child_comm_fd, F_SETFD, FD_CLOEXEC) != 0) {
			std::perror("fcntl(programmed_info.child_comm_fd, F_SETFD, FD_CLOEXEC)");
			std::exit(EXIT_FAILURE);
		}
	} else {
		std::perror("fork");
		std::exit(EXIT_FAILURE);
	}
}

int programmed_only(OspreyConfig& config) {
	ProgrammedProcessInfo programmed_info;

	launch_programmed_process(programmed_info, config);

	/* Reap the child. */
	int child_status;
	if (waitpid(programmed_info.child_pid, &child_status, 0) != programmed_info.child_pid) {
		std::perror("waitpid");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

int speculative_and_programmed(OspreyConfig& config) {
	SpeculativeProcessInfo speculative_info;
	ProgrammedProcessInfo programmed_info;

	/* Setup overlay for speculative process. */
	std::unique_ptr<osprey::util::Overlay> speculative_overlay;
	if (!config.no_overlay) {
		speculative_overlay = osprey::util::Overlay::create_overlay();
	}

	launch_speculative_process(speculative_info, config, speculative_overlay.get());
	std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	launch_programmed_process(programmed_info, config);

	/* No more communication with the speculative child is necessary. */
	close(speculative_info.child_comm_fd);

	/* Reap the programmed child. */
	int child_status;
	if (waitpid(programmed_info.child_pid, &child_status, 0) != programmed_info.child_pid) {
		std::perror("waitpid");
		return EXIT_FAILURE;
	}

	/* Reap the speculative child. */
	if (waitpid(speculative_info.child_pid, &child_status, 0) != speculative_info.child_pid) {
		std::perror("waitpid");
		return EXIT_FAILURE;
	}

	/* Cleanup overlay. */
	if (!config.no_overlay) {
		speculative_overlay->cleanup_overlay(config.preserve_overlay_directories);
	}

	return EXIT_SUCCESS;
}

bool parse_osprey_args(OspreyConfig& config, int osprey_argc, char** osprey_argv) {
	config.osprey_name = osprey_argv[0];

	po::options_description global_opts("Global options");
	// clang-format off
    global_opts.add_options()
        ("help", "produce help message")
        ("speculative-only", po::bool_switch(&config.speculative_only), "only run speculative pass")
        ("programmed-only", po::bool_switch(&config.programmed_only), "only run programmed pass")
        ("cleanup-only", po::bool_switch(&config.cleanup_overlays), "only clean up overlays")
        ("tracing-algorithm", po::value<std::string>(&config.tracing_algorithm)->default_value("MICROSET"), "algorithm to use for extracting the memory access pattern (MICROSET or FIFO)")
        ("window-size", po::value<std::size_t>(&config.window_size)->default_value(2048), "window size to use for tracing")
        ("trace-filebase", po::value<std::string>(&config.trace_filebase), "write access pattern trace to file at specified path")
        ("trace-to-stdout", po::bool_switch(&config.trace_to_stdout), "write trace in human-readable form to stdout")
        ("programming-algorithm", po::value<std::string>(&config.programming_algorithm)->default_value("3PO"))
		("mem-limit-low", po::value<std::size_t>(&config.mem_limit_low)->default_value(131072), "low memory limit for 3PO in kilobytes")
		("mem-limit-high", po::value<std::size_t>(&config.mem_limit_high)->default_value(131072 * 3), "high memory limit for 3PO in kilobytes")
		("mem-limit-max", po::value<std::size_t>(&config.mem_limit_max)->default_value(131072 * 4), "max memory limit for 3PO in kilobytes")
        ("batch-size", po::value<std::size_t>(&config.batch_size)->default_value(8192), "batch size for prefetching")
        ("preserve-overlay-directories", po::bool_switch(&config.preserve_overlay_directories), "do not delete overlay directories");
	// clang-format on

	config.no_overlay = true;

	po::variables_map vm;
	po::parsed_options parsed = po::parse_command_line(osprey_argc, osprey_argv, global_opts);
	po::store(parsed, vm);
	po::notify(vm);

	if (vm.contains("help")) {
		print_usage(config.osprey_name, false);
		std::cout << global_opts << std::endl;
		return true;
	}

	/* Based on example here: https://en.cppreference.com/w/cpp/string/byte/toupper */
	std::transform(
		config.tracing_algorithm.begin(), config.tracing_algorithm.end(), config.tracing_algorithm.begin(),
		[](unsigned char c) {
			return std::toupper(c);
		}
	);
	std::transform(
		config.programming_algorithm.begin(), config.programming_algorithm.end(), config.programming_algorithm.begin(),
		[](unsigned char c) {
			return std::toupper(c);
		}
	);

	/* Validate arguments. */

	if (config.speculative_only && config.programmed_only) {
		std::cout << "--speculative-only and --programmed-only cannot be applied simultaneously" << std::endl;
		return true;
	}

	if (config.trace_filebase.empty()) {
		std::cout << "--trace-filebase is required" << std::endl;
		return true;
	}

	if (config.tracing_algorithm != "MICROSET" && config.tracing_algorithm != "FIFO") {
		std::cout << "Invalid value for --tracing-algorithm: must be MICROSET or FIFO" << std::endl;
		return true;
	}

	if (config.programming_algorithm == "WATCH_ONLY" || config.programming_algorithm == "WATCH_DEBUG") {
		if (!(config.speculative_only || config.programmed_only)) {
			std::cout << "WATCH_ONLY and WATCH_DEBUG is only allowed when programming without a trace" << std::endl;
			return true;
		}
	} else if (config.programming_algorithm != "3PO") {
		std::cout << "Invalid value for --programming-algorithm: must be 3PO (or a special debug value)" << std::endl;
		return true;
	}

	return false;
}

int main(int argc, char** argv) {
	if (argc == 1) {
		print_usage(argv[0]);
		return EXIT_FAILURE;
	}

	/*
	 * Ascertain which command-line arguments are for the Osprey driver
	 * program and which specify the program invocation. To do so, we
	 * look for the first argument that doesn't start with "-". That's
	 * assumed to be the start of the program invocation.
	 */
	int osprey_argc = argc;
	char** osprey_argv = argv;
	int program_argc = 0;
	char** program_argv = nullptr;
	for (int i = 1; i != argc; i++) {
		if (argv[i][0] != '-') {
			osprey_argc = i;
			osprey_argv = argv;
			program_argc = argc - i;
			program_argv = &argv[i];
			break;
		}
	}

	/* Parse arguments to the Osprey driver program. */
	OspreyConfig config;
	{
		bool exit_now = false;
		try {
			exit_now = parse_osprey_args(config, osprey_argc, osprey_argv);
		} catch (const po::error& e) {
			std::cout << "Error parsing Osprey configuration: " << e.what() << std::endl;
			print_usage(osprey_argv[0]);
			exit_now = true;
		} catch (const std::invalid_argument& e) {
			std::cout << e.what() << std::endl;
			print_usage(osprey_argv[0]);
			exit_now = true;
		}

		if (exit_now) {
			return EXIT_FAILURE;
		}

		config.program_argc = program_argc;
		config.program_argv = program_argv;
	}

	if (config.cleanup_overlays) {
		osprey::util::Overlay::delete_all_overlays();
		return EXIT_SUCCESS;
	}

	/*
	 * Exit if program invocation is missing. Wait until now so that --help is
	 * handled appropriately.
	 */
	if (config.program_argc == 0) {
		std::cout << "Error: missing program invocation" << std::endl;
		print_usage(osprey_argv[0]);
		return EXIT_FAILURE;
	}

	if (config.speculative_only) {
		return speculative_only(config);
	} else if (config.programmed_only) {
		return programmed_only(config);
	} else {
		return speculative_and_programmed(config);
	}
}
