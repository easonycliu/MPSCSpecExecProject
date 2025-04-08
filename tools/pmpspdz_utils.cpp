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

	std::chrono::high_resolution_clock::time_point encrypt_start = std::chrono::high_resolution_clock::now();
	std::vector<Ciphertext> input_data_encrypt;
	encrypt_file(keypair, input_data, input_data_encrypt);
	std::chrono::high_resolution_clock::time_point encrypt_end = std::chrono::high_resolution_clock::now();
	std::cout << "Encrypt time: "
			  << std::chrono::duration_cast<std::chrono::milliseconds>(encrypt_end - encrypt_start).count() << " ms"
			  << std::endl;

	std::vector<Ciphertext> output_data_encrypt;
	if (strcmp(problem_name, "pmpspdz_matrix_multiply") == 0) {
		pmpspdz_matrix_multiply(problem_size, keypair, input_data_encrypt, output_data_encrypt, thread_num);
	} else if (strcmp(problem_name, "pmpspdz_matrix_vector_multiply") == 0) {
		pmpspdz_matrix_vector_multiply(problem_size, keypair, input_data_encrypt, output_data_encrypt, thread_num);
	} else if (strcmp(problem_name, "pmpspdz_vector_multiply") == 0) {
		pmpspdz_vector_multiply(problem_size, keypair, input_data_encrypt, output_data_encrypt, thread_num);
	} else if (strcmp(problem_name, "pmpspdz_sum") == 0) {
		pmpspdz_sum(problem_size, keypair, input_data_encrypt, output_data_encrypt, thread_num);
	} else {
		std::cerr << "Unknown problem name" << std::endl;
		return 1;
	}

	std::chrono::high_resolution_clock::time_point calc_end = std::chrono::high_resolution_clock::now();
	std::cout << "Calc time: " << std::chrono::duration_cast<std::chrono::milliseconds>(calc_end - encrypt_end).count()
			  << " ms" << std::endl;

	std::vector<int> output_data;
	decrypt_file(keypair, output_data_encrypt, output_data);
	std::chrono::high_resolution_clock::time_point decrypt_end = std::chrono::high_resolution_clock::now();
	std::cout << "Decrypt time: "
			  << std::chrono::duration_cast<std::chrono::milliseconds>(decrypt_end - calc_end).count() << " ms"
			  << std::endl;

	std::cout << "Total time: "
			  << std::chrono::duration_cast<std::chrono::milliseconds>(decrypt_end - encrypt_start).count() << " ms"
			  << std::endl;

	write_to_file<bs>(output_file, output_data);
}
