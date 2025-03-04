#include <fcntl.h>
#include <signal.h>

#include <algorithm>
#include <atomic>
#include <bitset>
#include <chrono>
#include <csetjmp>
#include <cstdint>
#include <iostream>
#include <stack>
#include <string>
#include <vector>

#include "emp-agmpc/emp-agmpc.h"
#include "emp-tool/emp-tool.h"

using namespace emp;

std::atomic<bool> exec;

std::jmp_buf jmp_env;

void signal_handler(int signal) {
    if (signal == SIGUSR2) {
        exec.store(true);
    } else if (signal == SIGINT) {
        longjmp(jmp_env, 1);
    }
    return;
}

template <std::size_t N>
void push(std::vector<std::uint8_t>& vec, std::bitset<N> val) {
    for (std::size_t i = 0; i < N; ++i) {
        vec.push_back(val[i]);
    }
}

template <std::size_t N>
std::bitset<N> pop(std::vector<std::uint8_t>& vec) {
    std::bitset<N> res(0);

    if (vec.size() < N) {
        return 0;
    }

    for (std::size_t i = N; i != 0; --i) {
        res[i - 1] = vec.back();
        vec.pop_back();
    }

    return res;
}

CMPC* setup(int party, int port, std::size_t party_num, std::string circuit_file, NetIOMP** io, NetIOMP** io2, CircuitFile** cf) {
    char* io_ips[] = {"", "127.0.0.1", "127.0.0.1"};
    *io = new NetIOMP(party, port, party_num, io_ips);
    *io2 = new NetIOMP(party, port + 2 * (party_num + 1) * (party_num + 1) + 1, party_num, io_ips);

    NetIOMP* ios[2] = {*io, *io2};

    *cf = new CircuitFile(circuit_file.c_str());

    CMPC* mpc = new CMPC(ios, party, *cf, party_num);
    std::cout << "Setup:\t" << party << std::endl;

    mpc->function_independent();
    std::cout << "FUNC_IND:\t" << party << std::endl;

    mpc->function_dependent();
    std::cout << "FUNC_DEP:\t" << party << std::endl;

    return mpc;
}

template <std::size_t key_bits, std::size_t value_bits>
void generate_merge_sorted_input(int party, std::size_t party_num, std::size_t problem_size,
                                 std::vector<std::uint8_t>& in) {
    std::size_t array_size = problem_size;
    for (std::size_t i = 0; i < problem_size; ++i) {
        if (party == 1) {
            push(in, std::bitset<key_bits>(i * party_num));
        } else {
            push(in, std::bitset<key_bits>(0));
        }
        push(in, std::bitset<value_bits>(0));
    }
    for (int party_id = 1; party_id < static_cast<int>(party_num); ++party_id) {
        for (std::size_t i = array_size; i != 0; --i) {
            if ((party_id + 1) == party) {
                if ((problem_size + i) > array_size) {
                    push(in, std::bitset<key_bits>((problem_size + i - array_size) * party_num - party_id));
                } else {
                    push(in, std::bitset<key_bits>(0));
                }
            } else {
                push(in, std::bitset<key_bits>(0));
            }
            push(in, std::bitset<value_bits>(0));
        }
        array_size *= 2;
    }
    std::cout << "Input size: " << in.size() << std::endl;
}

template <std::size_t key_bits, std::size_t value_bits>
void generate_full_sort_input(int party, std::size_t party_num, std::size_t problem_size,
                              std::vector<std::uint8_t>& in) {
    for (int party_id = 1; party_id <= static_cast<int>(party_num); ++party_id) {
        for (std::size_t i = problem_size; i != 0; --i) {
            if (party_id == party) {
                push(in, std::bitset<key_bits>(i * party_num - party_id));
            } else {
                push(in, std::bitset<key_bits>(0));
            }
            push(in, std::bitset<value_bits>(0));
        }
    }
}

void output(std::vector<std::uint8_t>& out, std::string output_path) {
    std::vector<std::bitset<64>> res;
    while (out.size() >= 64) {
        res.push_back(pop<64>(out));
    }

    std::reverse(res.begin(), res.end());

    int output_fd = open(output_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (output_fd == -1) {
        std::cerr << "Failed to open output file" << std::endl;
        return;
    }

    if (write(output_fd, res.data(), res.size() * sizeof(std::bitset<64>)) == -1) {
        std::cerr << "Failed to write to output file" << std::endl;
    }

    if (close(output_fd) == -1) {
        std::cerr << "Failed to close output file" << std::endl;
    }
}

int main(int argc, char** argv) {
    if (argc != 7) {
        std::cout << "Usage: " << argv[0] << " <party> <port> <party_num> <problem_name> <circuit_path> <output_path>"
                  << std::endl;
        return 0;
    }

    constexpr std::size_t key_bits = 32;
    constexpr std::size_t value_bits = 128;

    int party, port;
    parse_party_and_port(argv, &party, &port);
    std::size_t party_num = static_cast<std::size_t>(std::atoll(argv[3]));
    std::string problem_name = argv[4];
    std::string circuit_path = argv[5];
    std::string output_path = argv[6];

    signal(SIGUSR2, signal_handler);
    signal(SIGINT, signal_handler);

    NetIOMP *io, *io2;
    CircuitFile* cf;
    CMPC* mpc = setup(party, port, party_num, circuit_path, &io, &io2, &cf);

    std::vector<std::uint8_t> in;
    in.reserve(cf->n1 + cf->n2);
    std::vector<std::uint8_t> out;

    if (problem_name == "merge_sorted") {
        generate_merge_sorted_input<key_bits, value_bits>(party, party_num, cf->n1 / (key_bits + value_bits), in);
    } else if (problem_name == "full_sort") {
        generate_full_sort_input<key_bits, value_bits>(party, party_num, cf->n1 / (key_bits + value_bits), in);
    } else {
        std::cout << "Invalid problem name" << std::endl;
        return 0;
    }

    if (setjmp(jmp_env) == 0) {
        while (true) {
            while (!exec.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            exec.store(false);

            std::cout << "Starting online computing" << std::endl;

            out.resize(cf->n3);
            std::chrono::time_point start = std::chrono::high_resolution_clock::now();
            mpc->online(reinterpret_cast<bool*>(in.data()), reinterpret_cast<bool*>(out.data()));
            std::chrono::time_point end = std::chrono::high_resolution_clock::now();

            std::cout << "Total time: " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
                      << std::endl;

            if (party == 1) {
                std::cout << "Output size: " << out.size() << std::endl;
                output(out, output_path);
            }
        }
    } else {
        std::cout << "Exiting" << std::endl;
    }

	delete mpc;
	delete io;
	delete io2;
	delete cf;

    return 0;
}
