#include <array>
#include <chrono>
#include <fstream>
#include <iostream>
#include <numeric>
#include <string>
#include <thread>
#include <vector>

#include "FHE/Ciphertext.h"
#include "FHE/FHE_Keys.h"
#include "FHE/FHE_Params.h"
#include "FHE/Plaintext.h"
#include "Math/bigint.h"
#include "Math/modp.hpp"
#include "Tools/random.h"

#include "mapreduce.hpp"

class Problem {
public:
	Problem() = default;
	virtual ~Problem() = default;
	virtual void divide(std::size_t workers, std::size_t problem_size, const std::vector<int>& input_data, std::vector<std::pair<std::size_t, std::vector<int>>>& divide_input_data) = 0;
	virtual void calculate(std::size_t problem_size, FHE_KeyPair& keypair, const std::vector<Ciphertext>& input_data, std::vector<Ciphertext>& output_data) = 0;
	virtual void aggregate(const std::vector<std::vector<int>>& partial_output_data, std::vector<int>& output_data) = 0;
};

Ciphertext to_ciphertext(FHE_KeyPair& keypair, int value) {
	Plaintext_mod_prime plaintext(keypair.pk.get_params().get_plaintext_field_data<FFT_Data>());
	plaintext.assign_constant(value);

	return keypair.pk.encrypt(plaintext);
}

int from_ciphertext(FHE_KeyPair& keypair, const Ciphertext& ciphertext) {
	Plaintext_mod_prime plaintext = keypair.sk.decrypt(ciphertext);
	return bigint(plaintext.element(0)).get_si();
}

template <std::size_t bs>
bool read_from_file(const std::string& file, std::vector<int>& data) {
	std::ifstream stream(file, std::ios::binary);
	if (!stream.is_open()) {
		std::cout << "Open file failed" << std::endl;
		return false;
	}

	std::size_t read_size = 0;
	std::array<int, bs> buffer;

	while ((read_size = stream.readsome(reinterpret_cast<char*>(buffer.data()), sizeof(buffer))) > 0) {
		data.insert(data.end(), buffer.begin(), buffer.begin() + read_size / sizeof(int));
	}

	return true;
}

template <std::size_t bs>
bool write_to_file(const std::string& file, const std::vector<int>& data) {
	std::ofstream stream(file, std::ios::binary);
	if (!stream.is_open()) {
		std::cout << "Open file failed" << std::endl;
		return false;
	}

	std::array<int, bs> buffer;
	stream.rdbuf()->pubsetbuf(reinterpret_cast<char*>(buffer.data()), sizeof(buffer));

	for (int item : data) {
		stream.write(reinterpret_cast<const char*>(&item), sizeof(item));
	}

	stream.flush();
	stream.close();

	return true;
}

void encrypt_file(FHE_KeyPair& keypair, const std::vector<int>& input_data, std::vector<Ciphertext>& output_data) {
	output_data.clear();
	output_data.reserve(input_data.size());
	for (int item : input_data) {
		output_data.push_back(to_ciphertext(keypair, item));
	}
}

void decrypt_file(FHE_KeyPair& keypair, const std::vector<Ciphertext>& input_data, std::vector<int>& output_data) {
	for (const Ciphertext& item : input_data) {
		int value = from_ciphertext(keypair, item);
		output_data.push_back(value);
	}
}

class VectorMultiply : public Problem {
public:
	VectorMultiply() = default;

	void divide(std::size_t workers, std::size_t problem_size, const std::vector<int>& input_data, std::vector<std::pair<std::size_t, std::vector<int>>>& divide_input_data) override {
		if (input_data.size() != problem_size * 2) {
			std::cerr << "Input data size must be twice the problem size." << std::endl;
			return;
		}
		std::size_t vector_size = input_data.size() / 2;
		std::size_t vector_size_per_worker = vector_size / workers + std::size_t(vector_size % workers != 0);
		for (std::size_t i = 0; i < workers; ++i) {
			std::size_t start = i * vector_size_per_worker;
			std::size_t end = std::min(start + vector_size_per_worker, vector_size);
			std::vector<int> chunk(input_data.begin() + start, input_data.begin() + end);
			chunk.insert(chunk.end(), input_data.begin() + vector_size + start, input_data.begin() + vector_size + end);
			divide_input_data.emplace_back(end - start, std::move(chunk));
		}
	}

	void calculate(std::size_t problem_size, FHE_KeyPair& keypair, const std::vector<Ciphertext>& input_data, std::vector<Ciphertext>& output_data) override {
		if (input_data.size()  != problem_size * 2) {
			std::cerr << "Input data size must be twice the problem size." << std::endl;
			return;
		}

		std::size_t vector_size = input_data.size() / 2;
		output_data.resize(1, to_ciphertext(keypair, 0).mul(keypair.pk, to_ciphertext(keypair, 1)));
		for (std::size_t i = 0; i < vector_size; ++i) {
			output_data[0] += input_data[i].mul(keypair.pk, input_data[vector_size + i]);
		}
	}

	void aggregate(const std::vector<std::vector<int>>& partial_output_data, std::vector<int>& output_data) override {
		output_data.resize(1, 0);
		for (const std::vector<int>& one_partial_output_data : partial_output_data) {
			for (int item : one_partial_output_data) {
				output_data[0] += item;
			}
		}
	}
};

// Summation Logic Using MapReduce (Parallelized)
void pmpspdz_sum(
	std::size_t problem_size, FHE_KeyPair& keypair, const std::vector<Ciphertext>& input_data,
	std::vector<Ciphertext>& output_data, std::size_t num_threads
) {
	const std::size_t chunk_size = std::max<std::size_t>(1, input_data.size() / num_threads);
	std::vector<std::vector<std::size_t>> chunks;

	for (std::size_t i = 0; i < input_data.size(); i += chunk_size) {
		std::size_t end = std::min(i + chunk_size, input_data.size());
		std::vector<std::size_t> chunk(end - i);
		std::iota(chunk.begin(), chunk.end(), i);
		chunks.push_back(chunk);
	}

	std::vector<Ciphertext> partial_sums;
	partial_sums.resize(chunks.size(), to_ciphertext(keypair, 0));
	std::vector<std::size_t> partial_sums_indexes(chunks.size());
	std::iota(partial_sums_indexes.begin(), partial_sums_indexes.end(), 0);
	MapReduce::map<std::vector<std::size_t>, std::size_t>(
		chunks, partial_sums_indexes,
		[&](const std::vector<std::size_t>& chunk, std::size_t& index) {
			for (std::size_t i : chunk) {
				partial_sums[index] += input_data[i];
			}
		},
		num_threads
	);

	output_data.resize(1, to_ciphertext(keypair, 0));
	MapReduce::reduce<Ciphertext>(
		partial_sums, output_data[0],
		[](const Ciphertext& a, const Ciphertext& b) -> Ciphertext {
			return a + b;
		}
	);
}

void pmpspdz_vector_multiply(
	std::size_t problem_size, FHE_KeyPair& keypair, const std::vector<Ciphertext>& input_data,
	std::vector<Ciphertext>& output_data, std::size_t num_threads
) {
	if (input_data.size() % 2 != 0) {
		std::cerr << "Input data size must be even" << std::endl;
		std::abort();
	}

	std::size_t vector_size = input_data.size() / 2;
	std::size_t elements_per_thread = vector_size / num_threads + int(vector_size % num_threads != 0);

	std::vector<Ciphertext> partial_sums(num_threads, to_ciphertext(keypair, 0).mul(keypair.pk, to_ciphertext(keypair, 1)));

	std::vector<std::thread> threads;

	for (std::size_t i = 0; i < num_threads - 1; ++i) {
		threads.emplace_back(std::thread([&]() {
			std::size_t start = i * elements_per_thread;
			std::size_t end = std::min(start + elements_per_thread, vector_size);
			for (std::size_t j = start; j < end; ++j) {
				partial_sums[i] += input_data[j].mul(keypair.pk, input_data[vector_size + j]);
			}
		}));
	}

	std::size_t start = (num_threads - 1) * elements_per_thread;
	std::size_t end = vector_size;
	for (std::size_t j = start; j < end; ++j) {
		partial_sums[num_threads - 1] += input_data[j].mul(keypair.pk, input_data[vector_size + j]);
	}

	for (auto& thread : threads) {
		thread.join();
	}

	output_data.resize(1, to_ciphertext(keypair, 0).mul(keypair.pk, to_ciphertext(keypair, 1)));
	for (const auto& partial_sum : partial_sums) {
		output_data[0] += partial_sum;
	}
}

void pmpspdz_matrix_vector_multiply(
	std::size_t problem_size, FHE_KeyPair& keypair, const std::vector<Ciphertext>& input_data,
	std::vector<Ciphertext>& output_data, std::size_t num_threads
) {
	if (input_data.size() != problem_size * problem_size + problem_size) {
		std::cerr << "Input data size does not match problem size" << std::endl;
		std::abort();
	}

	std::vector<std::size_t> tasks(problem_size);
	std::iota(tasks.begin(), tasks.end(), 0);

	output_data.resize(problem_size, to_ciphertext(keypair, 0).mul(keypair.pk, to_ciphertext(keypair, 1)));
	MapReduce::map<std::size_t, Ciphertext>(
		tasks, output_data,
		[&](const std::size_t& i, Ciphertext& output) {
			for (std::size_t j = 0; j < problem_size; j++) {
				output += input_data[j].mul(keypair.pk, input_data[problem_size + i * problem_size + j]);
			}
		},
		num_threads
	);
}

// Matrix Multiplication Logic Using MapReduce
void pmpspdz_matrix_multiply(
	std::size_t problem_size, FHE_KeyPair& keypair, const std::vector<Ciphertext>& input_data,
	std::vector<Ciphertext>& output_data, std::size_t num_threads
) {
	if (input_data.size() != problem_size * problem_size * 2) {
		std::cerr << "Input data size does not match problem size" << std::endl;
		std::abort();
	}

	std::vector<std::pair<std::size_t, std::size_t>> tasks;
	for (std::size_t i = 0; i < problem_size; i++) {
		for (std::size_t j = 0; j < problem_size; j++) {
			tasks.emplace_back(i, j);
		}
	}

	output_data.resize(problem_size * problem_size, to_ciphertext(keypair, 0));
	MapReduce::map<std::pair<std::size_t, std::size_t>, Ciphertext>(
		tasks, output_data,
		[&](const std::pair<std::size_t, std::size_t>& task) -> Ciphertext {
			std::size_t i = task.first;
			std::size_t j = task.second;
			Ciphertext result =
				input_data[i * problem_size].mul(keypair.pk, input_data[problem_size * problem_size + j]);
			for (std::size_t k = 1; k < problem_size; k++) {
				result += input_data[i * problem_size + k].mul(
					keypair.pk, input_data[problem_size * problem_size + k * problem_size + j]
				);
			}
			return result;
		},
		num_threads
	);
}

int main(int argc, char** argv) {
	if (argc != 6) {
		std::cout << "Usage: " << argv[0] << " [problem_name] [problem_size] [thread_num] [input_file] [output_file]"
				  << std::endl;
		return 1;
	}

	char* problem_name = argv[1];
	std::size_t problem_size = std::stoull(argv[2]);
	std::size_t thread_num = std::stoull(argv[3]);
	char* input_file = argv[4];
	char* output_file = argv[5];

	constexpr std::size_t bs = 4096;

	// Initialize parameters
	FHE_Params params;
	params.basic_generation_mod_prime(64); // Example bit length for prime

	// Generate keys
	FHE_KeyPair keypair(params);
	keypair.generate();

	std::vector<int> input_data;
	read_from_file<bs>(input_file, input_data);
	std::unique_ptr<Problem> problem;

	if (strcmp(problem_name, "pmpspdz_vector_multiply") == 0) {
		problem = std::make_unique<VectorMultiply>();
	} else {
		std::cerr << "Unknown problem name: " << problem_name << std::endl;
		return 1;
	}

	std::vector<std::pair<std::size_t, std::vector<int>>> divide_input_data;
	problem->divide(thread_num, problem_size, input_data, divide_input_data);
	std::vector<std::vector<int>> partial_output_data(divide_input_data.size());

	std::array<std::pair<std::string, std::vector<std::atomic<bool>>>, 3> progress;
	progress[0].first = "Encrypt";
	progress[0].second = std::vector<std::atomic<bool>>(thread_num);
	progress[1].first = "Calculate";
	progress[1].second = std::vector<std::atomic<bool>>(thread_num);
	progress[2].first = "Decrypt";
	progress[2].second = std::vector<std::atomic<bool>>(thread_num);
	std::vector<std::thread> threads(thread_num);
	for (std::size_t i = 0; i < thread_num; ++i) {
		threads.emplace_back([&, i]() {
			std::vector<Ciphertext> encrypt_input_data;
			encrypt_file(keypair, divide_input_data[i].second, encrypt_input_data);
			progress[0].second[i] = true;

			std::vector<Ciphertext> encrypt_output_data;
			problem->calculate(divide_input_data[i].first, keypair, encrypt_input_data, encrypt_output_data);
			progress[1].second[i] = true;

			decrypt_file(keypair, encrypt_output_data, partial_output_data[i]);
			progress[2].second[i] = true;
		});
	}

	std::chrono::time_point<std::chrono::high_resolution_clock> total_start = std::chrono::high_resolution_clock::now();
	for (std::size_t i = 0; i < progress.size(); ++i) {
		std::chrono::time_point<std::chrono::high_resolution_clock> start = std::chrono::high_resolution_clock::now();
		while (true) {
			bool done = true;
			for (std::size_t j = 0; j < thread_num; ++j) {
				if (!progress[i].second[j]) {
					done = false;
					break;
				}
			}
			if (done) {
				break;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
		std::chrono::time_point<std::chrono::high_resolution_clock> end = std::chrono::high_resolution_clock::now();
		std::cout << progress[i].first << " time: "
				  << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
				  << " milliseconds" << std::endl;
	}
	std::chrono::time_point<std::chrono::high_resolution_clock> total_end = std::chrono::high_resolution_clock::now();
	std::cout << "Total time: "
			  << std::chrono::duration_cast<std::chrono::milliseconds>(total_end - total_start).count()
			  << " milliseconds" << std::endl;

	for (auto& thread : threads) {
		if (thread.joinable()) {
			thread.join();
		}
	}

	std::vector<int> output_data;
	problem->aggregate(partial_output_data, output_data);
	write_to_file<bs>(output_file, output_data);

	return 0;
}
