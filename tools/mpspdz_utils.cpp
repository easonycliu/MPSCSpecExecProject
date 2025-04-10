#include <array>
#include <chrono>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "FHE/Ciphertext.h"
#include "FHE/FHE_Keys.h"
#include "FHE/FHE_Params.h"
#include "FHE/Plaintext.h"
#include "Math/bigint.h"
#include "Math/modp.hpp"
#include "Tools/random.h"

#include "util.hpp"

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

void mpspdz_vector_multiply(
	std::size_t problem_size, FHE_KeyPair& keypair, const std::vector<Ciphertext>& input_data,
	std::vector<Ciphertext>& output_data
) {
	if (input_data.size() != problem_size * 2) {
		std::cerr << "Input data size does not match problem size" << std::endl;
		std::abort();
	}

	output_data.clear();
	output_data.reserve(problem_size);
	for (std::size_t i = 0; i < problem_size; i++) {
		output_data.push_back(input_data[i].mul(keypair.pk, input_data[i + problem_size]));
	}
}

void mpspdz_matrix_multiply(
	std::size_t problem_size, FHE_KeyPair& keypair, const std::vector<Ciphertext>& input_data,
	std::vector<Ciphertext>& output_data
) {
	if (input_data.size() != problem_size * problem_size * 2) {
		std::cerr << "Input data size does not match problem size" << std::endl;
		std::abort();
	}

	std::vector<std::vector<Ciphertext>> matrix_a(problem_size, std::vector<Ciphertext>());
	std::vector<std::vector<Ciphertext>> matrix_b(problem_size, std::vector<Ciphertext>());
	for (std::size_t i = 0; i < problem_size; i++) {
		for (std::size_t j = 0; j < problem_size; j++) {
			matrix_a[i].push_back(input_data[i * problem_size + j]);
			matrix_b[i].push_back(input_data[problem_size * problem_size + i * problem_size + j]);
		}
	}

	std::vector<std::vector<Ciphertext>> matrix_output(problem_size, std::vector<Ciphertext>());
	for (std::size_t i = 0; i < problem_size; i++) {
		for (std::size_t j = 0; j < problem_size; j++) {
			matrix_output[i].push_back(matrix_a[i][0].mul(keypair.pk, matrix_b[0][j]));
			for (std::size_t k = 1; k < problem_size; k++) {
				matrix_output[i][j] += matrix_a[i][k].mul(keypair.pk, matrix_b[k][j]);
			}
			std::cerr << "Calc: " << i << " " << j << std::endl;
		}
	}

	output_data.clear();
	output_data.reserve(problem_size * problem_size);
	for (std::size_t i = 0; i < problem_size; i++) {
		for (std::size_t j = 0; j < problem_size; j++) {
			output_data.push_back(matrix_output[i][j]);
		}
	}
}

void mpspdz_matrix_vector_multiply(
	std::size_t problem_size, FHE_KeyPair& keypair, const std::vector<Ciphertext>& input_data,
	std::vector<Ciphertext>& output_data
) {
	if (input_data.size() != problem_size * problem_size + problem_size) {
		std::cerr << "Input data size does not match problem size" << std::endl;
		std::abort();
	}

	std::vector<Ciphertext> vector;
	std::vector<std::vector<Ciphertext>> matrix(problem_size, std::vector<Ciphertext>());
	for (std::size_t i = 0; i < problem_size; i++) {
		vector.push_back(input_data[i]);
	}
	for (std::size_t i = 0; i < problem_size; i++) {
		for (std::size_t j = 0; j < problem_size; j++) {
			matrix[i].push_back(input_data[problem_size + i * problem_size + j]);
		}
	}

	output_data.clear();
	output_data.reserve(problem_size);
	for (std::size_t i = 0; i < problem_size; i++) {
		output_data.push_back(matrix[i][0].mul(keypair.pk, vector[0]));
		for (std::size_t j = 1; j < problem_size; j++) {
			output_data[i] += matrix[i][j].mul(keypair.pk, vector[j]);
		}
	}
}

int main(int argc, char** argv) {
	if (argc != 5) {
		std::cout << "Usage: " << argv[0] << " [problem_name] [problem_size] [input_file] [output_file]" << std::endl;
		return 1;
	}

	char* problem_name = argv[1];
	std::size_t problem_size = std::stoull(argv[2]);
	char* input_file = argv[3];
	char* output_file = argv[4];

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

	double start_calc_cpu_time = get_cpu_time_ms();

	std::vector<Ciphertext> output_data_encrypt;
	if (strcmp(problem_name, "mpspdz_matrix_multiply") == 0) {
		mpspdz_matrix_multiply(problem_size, keypair, input_data_encrypt, output_data_encrypt);
	} else if (strcmp(problem_name, "mpspdz_matrix_vector_multiply") == 0) {
		mpspdz_matrix_vector_multiply(problem_size, keypair, input_data_encrypt, output_data_encrypt);
	} else {
		std::cerr << "Unknown problem name" << std::endl;
		return 1;
	}

	double end_calc_cpu_time = get_cpu_time_ms();

	std::chrono::high_resolution_clock::time_point calc_end = std::chrono::high_resolution_clock::now();
	std::cout << "Calc cpu time: " << end_calc_cpu_time - start_calc_cpu_time << " ms" << std::endl;
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
