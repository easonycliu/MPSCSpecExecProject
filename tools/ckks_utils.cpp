/*
 * Copyright (C) 2020 Sam Kumar <samkumar@cs.berkeley.edu>
 * Copyright (C) 2020 University of California, Berkeley
 * All rights reserved.
 *
 * This file is part of MAGE.
 *
 * MAGE is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * MAGE is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with MAGE.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <seal/seal.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <string>
#include <thread>

#include "util/binaryfile.hpp"

double ckks_scale = std::pow(2.0, 40);

template <std::size_t bs, typename T>
bool read_from_file(const std::string& file, std::vector<T>& data) {
	std::ifstream stream(file, std::ios::binary);
	if (!stream.is_open()) {
		std::cout << "Open file failed" << std::endl;
		return false;
	}

	std::size_t read_size = 0;
	std::array<T, bs> buffer;

	while ((read_size = stream.readsome(reinterpret_cast<char*>(buffer.data()), sizeof(buffer))) > 0) {
		data.insert(data.end(), buffer.begin(), buffer.begin() + read_size / sizeof(T));
	}

	return true;
}

template <std::size_t bs, typename T>
bool write_to_file(const std::string& file, const std::vector<T>& data) {
	std::ofstream stream(file, std::ios::binary);
	if (!stream.is_open()) {
		std::cout << "Open file failed" << std::endl;
		return false;
	}

	std::array<T, bs> buffer;
	stream.rdbuf()->pubsetbuf(reinterpret_cast<char*>(buffer.data()), sizeof(buffer));

	for (const T& item : data) {
		stream.write(reinterpret_cast<const char*>(&item), sizeof(item));
	}

	stream.flush();
	stream.close();

	return true;
}

std::tuple<seal::EncryptionParameters, seal::SecretKey, seal::PublicKey, seal::RelinKeys, seal::GaloisKeys> keygen() {
	seal::EncryptionParameters parms(seal::scheme_type::ckks);
	size_t poly_modulus_degree = 8192;
	parms.set_poly_modulus_degree(poly_modulus_degree);
	parms.set_coeff_modulus(seal::CoeffModulus::Create(poly_modulus_degree, {60, 40, 40, 60}));

	seal::SEALContext context(parms);
	seal::KeyGenerator keygen(context);
	seal::SecretKey secret_key = keygen.secret_key();
	seal::PublicKey public_key;
	keygen.create_public_key(public_key);

	seal::RelinKeys relin_keys;
	keygen.create_relin_keys(relin_keys);

	seal::GaloisKeys gal_keys;
	keygen.create_galois_keys(gal_keys);

	return {parms, secret_key, public_key, relin_keys, gal_keys};
}

template <typename T>
seal::Ciphertext to_ciphertext(
	std::shared_ptr<const seal::SEALContext::ContextData>& context_data, seal::Encryptor& encryptor,
	seal::CKKSEncoder& encoder, const T& input, std::size_t level
) {
	seal::parms_id_type target_level_parms_id = context_data->parms_id();

	seal::Plaintext plaintext;
	encoder.encode(input, target_level_parms_id, ckks_scale, plaintext);

	seal::Ciphertext ciphertext;
	encryptor.encrypt(plaintext, ciphertext);
	return ciphertext;
}

template <typename T>
T from_ciphertext(seal::Decryptor& decryptor, seal::CKKSEncoder& encoder, const seal::Ciphertext& input) {
	seal::Plaintext plaintext;
	decryptor.decrypt(input, plaintext);

	std::vector<T> value;
	encoder.decode(plaintext, value);
	return value[0];
}

template <typename T>
void encrypt_file(
	seal::EncryptionParameters& parms, seal::PublicKey& public_key, const std::vector<T>& input_data,
	std::vector<seal::Ciphertext>& output_data, std::size_t level
) {
	seal::SEALContext context(parms);
	seal::Encryptor encryptor(context, public_key);

	seal::CKKSEncoder encoder(context);

	std::shared_ptr<const seal::SEALContext::ContextData> context_data = context.first_context_data();
	while (context_data->chain_index() > level) {
		context_data = context_data->next_context_data();
	}
	if (context_data->chain_index() != level) {
		std::cout << "Could not find params for level " << level << std::endl;
		std::abort();
	}

	for (const T& item : input_data) {
		output_data.push_back(to_ciphertext(context_data, encryptor, encoder, item, level));
	}
}

template <typename T>
void decrypt_file(
	seal::EncryptionParameters& parms, seal::SecretKey& secret_key, const std::vector<seal::Ciphertext>& input_data,
	std::vector<T>& output_data
) {
	seal::SEALContext context(parms);
	seal::Decryptor decryptor(context, secret_key);

	seal::CKKSEncoder encoder(context);

	for (const seal::Ciphertext& item : input_data) {
		output_data.push_back(from_ciphertext<T>(decryptor, encoder, item));
	}
}

void real_sum(
	seal::EncryptionParameters& parms, seal::RelinKeys& relin_keys, std::size_t problem_size, std::size_t round_num,
	const std::vector<seal::Ciphertext>& input_data, std::vector<seal::Ciphertext>& output_data
) {
	seal::SEALContext context(parms);
	seal::Evaluator evaluator(context);

	output_data.clear();
	output_data.resize(1);

	seal::Ciphertext& sum = output_data[0];
	sum = input_data[0];

	for (std::size_t i = 0; i != round_num; i++) {
		for (std::size_t j = 0; j != problem_size; j++) {
			evaluator.add_inplace(sum, input_data[j]);
		}
	}
}

void real_statistics(
	seal::EncryptionParameters& parms, seal::RelinKeys& relin_keys, std::size_t problem_size, std::size_t round_num,
	const std::vector<seal::Ciphertext>& input_data, std::vector<seal::Ciphertext>& output_data
) {
	seal::SEALContext context(parms);
	seal::Evaluator evaluator(context);

	output_data.clear();
	output_data.resize(2);

	seal::Ciphertext& sum = output_data[0];
	seal::Ciphertext& sum_squares = output_data[1];

	sum = input_data[0];

	seal::Ciphertext temp_square;
	seal::Ciphertext first_square;
	evaluator.square(input_data[0], sum_squares);
	evaluator.square(input_data[0], first_square);

	for (std::size_t i = 0; i != round_num; i++) {
		for (std::size_t j = 0; j != problem_size; j++) {
			evaluator.add_inplace(sum, input_data[j]);
			evaluator.square(input_data[j], temp_square);
			evaluator.add_inplace(sum_squares, temp_square);
		}
	}

	evaluator.sub_inplace(sum_squares, first_square);
	evaluator.sub_inplace(sum, input_data[0]);
}

void real_matrix_vector_multiply(
	seal::EncryptionParameters& parms, seal::RelinKeys& relin_keys, std::size_t problem_size,
	const std::vector<seal::Ciphertext>& input_data, std::vector<seal::Ciphertext>& output_data
) {
	seal::SEALContext context(parms);
	seal::Evaluator evaluator(context);

	output_data.clear();
	output_data.resize(problem_size);

	std::span<const seal::Ciphertext> input_points_vector(input_data.data(), problem_size);
	std::span<const seal::Ciphertext> input_points_matrix(
		input_data.data() + problem_size, problem_size * problem_size
	);

	for (std::size_t i = 0; i != problem_size; i++) {
		evaluator.multiply(input_points_matrix[i * problem_size], input_points_vector[0], output_data[i]);
		for (std::size_t j = 1; j != problem_size; j++) {
			seal::Ciphertext temp;
			evaluator.multiply(input_points_matrix[i * problem_size + j], input_points_vector[j], temp);
			evaluator.add_inplace(output_data[i], temp);
		}
	}
}

void real_naive_matrix_multiply(
	seal::EncryptionParameters& parms, seal::RelinKeys& relin_keys, std::size_t problem_size,
	const std::vector<seal::Ciphertext>& input_data, std::vector<seal::Ciphertext>& output_data
) {
	seal::SEALContext context(parms);
	seal::Evaluator evaluator(context);

	output_data.clear();
	output_data.resize(problem_size * problem_size);

	std::span<const seal::Ciphertext> input_points_a(input_data.data(), problem_size * problem_size);
	std::span<const seal::Ciphertext> input_points_b(
		input_data.data() + problem_size * problem_size, problem_size * problem_size
	);

	for (std::size_t row_a = 0; row_a != problem_size; row_a++) {
		for (std::size_t col_b = 0; col_b != problem_size; col_b++) {
			evaluator.multiply(
				input_points_a[row_a * problem_size], input_points_b[col_b * problem_size],
				output_data[row_a * problem_size + col_b]
			);
			for (std::size_t i = 1; i != problem_size; i++) {
				seal::Ciphertext temp;
				evaluator.multiply(
					input_points_a[row_a * problem_size + i], input_points_b[col_b * problem_size + i], temp
				);
				evaluator.add_inplace(output_data[row_a * problem_size + col_b], temp);
			}
		}
	}
}

void real_tiled_matrix_multiply(
	seal::EncryptionParameters& parms, seal::RelinKeys& relin_keys, std::size_t problem_size,
	const std::vector<seal::Ciphertext>& input_data, std::vector<seal::Ciphertext>& output_data
) {
	seal::SEALContext context(parms);
	seal::Evaluator evaluator(context);

	output_data.clear();
	output_data.resize(problem_size * problem_size);

	std::span<const seal::Ciphertext> input_points_a(input_data.data(), problem_size * problem_size);
	std::span<const seal::Ciphertext> input_points_b(
		input_data.data() + problem_size * problem_size, problem_size * problem_size
	);

	std::size_t memory_size = 256 * 1024 * 1024;
	std::size_t tile_size = std::max(((std::size_t) std::sqrt(memory_size)) / 2048, 1ul);

	for (std::size_t batch_row_a = 0; batch_row_a < problem_size; batch_row_a += tile_size) {
		for (std::size_t batch_col_b = 0; batch_col_b < problem_size; batch_col_b += tile_size) {
			for (std::size_t batch_cols_a_rows_b = 0; batch_cols_a_rows_b < problem_size;
				 batch_cols_a_rows_b += tile_size) {
				/* Multiply the submatrices. */
				for (std::size_t row_a = batch_row_a; row_a < problem_size && row_a < batch_row_a + tile_size;
					 row_a++) {
					for (std::size_t col_b = batch_col_b; col_b < problem_size && col_b < batch_col_b + tile_size;
						 col_b++) {
						/* This goes in result at row row_a and column col_b. */
						std::size_t i_batch = (row_a - batch_row_a) * tile_size + (col_b - batch_col_b);
						std::size_t dot_product_size = std::min(tile_size, problem_size - batch_cols_a_rows_b);
						seal::Ciphertext dot_product_result;
						evaluator.multiply(
							input_points_a[row_a * problem_size + batch_cols_a_rows_b],
							input_points_b[col_b * problem_size + batch_cols_a_rows_b], dot_product_result
						);
						for (std::size_t i = 1; i < dot_product_size; i++) {
							seal::Ciphertext temp;
							evaluator.multiply(
								input_points_a[row_a * problem_size + batch_cols_a_rows_b + i],
								input_points_b[col_b * problem_size + batch_cols_a_rows_b + i], temp
							);
							evaluator.add_inplace(dot_product_result, temp);
						}
						if (output_data[row_a * problem_size + col_b].size() != 0) {
							evaluator.add_inplace(output_data[row_a * problem_size + col_b], dot_product_result);
						} else {
							output_data[row_a * problem_size + col_b] = std::move(dot_product_result);
						}
					}
				}
			}
		}
	}
}

int main(int argc, char** argv) {
	if (argc != 5) {
		std::cout << "Usage: " << argv[0] << " [problem_name] [problem_size] [input_file] [output_file]" << std::endl;
		return 1;
	}

	constexpr std::size_t bs = 4096;

	std::string problem_name = argv[1];
	std::string problem_size_str = argv[2];
	std::string input_file = argv[3];
	std::string output_file = argv[4];

	std::size_t problem_size = std::stoull(problem_size_str.substr(0, problem_size_str.find_first_of(':')));
	std::size_t round_num = std::stoull(problem_size_str.substr(problem_size_str.find_first_of(':') + 1));

	std::size_t level = 0;
	if (problem_name == "real_sum") {
		level = 0;
	} else if (problem_name == "real_statistics") {
		level = 2;
	} else if (problem_name == "real_matrix_vector_multiply") {
		level = 1;
	} else if (problem_name == "real_naive_matrix_multiply") {
		level = 1;
	} else if (problem_name == "real_tiled_matrix_multiply") {
		level = 1;
	} else {
		std::cerr << "Unknown problem name" << std::endl;
		return 1;
	}

	std::tuple<seal::EncryptionParameters, seal::SecretKey, seal::PublicKey, seal::RelinKeys, seal::GaloisKeys>
		keypair = keygen();

	std::vector<double> input_data;
	read_from_file<bs>(input_file, input_data);

	std::chrono::high_resolution_clock::time_point encrypt_start = std::chrono::high_resolution_clock::now();
	std::vector<seal::Ciphertext> input_data_encrypt;
	encrypt_file(std::get<0>(keypair), std::get<2>(keypair), input_data, input_data_encrypt, level);
	std::chrono::high_resolution_clock::time_point encrypt_end = std::chrono::high_resolution_clock::now();
	std::cout << "Encrypt time: "
			  << std::chrono::duration_cast<std::chrono::milliseconds>(encrypt_end - encrypt_start).count() << " ms"
			  << std::endl;

	std::vector<seal::Ciphertext> output_data_encrypt;
	if (problem_name == "real_sum") {
		real_sum(
			std::get<0>(keypair), std::get<3>(keypair), problem_size, round_num, input_data_encrypt, output_data_encrypt
		);
	} else if (problem_name == "real_statistics") {
		real_statistics(
			std::get<0>(keypair), std::get<3>(keypair), problem_size, round_num, input_data_encrypt, output_data_encrypt
		);
	} else if (problem_name == "real_matrix_vector_multiply") {
		real_matrix_vector_multiply(
			std::get<0>(keypair), std::get<3>(keypair), problem_size, input_data_encrypt, output_data_encrypt
		);
	} else if (problem_name == "real_naive_matrix_multiply") {
		real_naive_matrix_multiply(
			std::get<0>(keypair), std::get<3>(keypair), problem_size, input_data_encrypt, output_data_encrypt
		);
	} else if (problem_name == "real_tiled_matrix_multiply") {
		real_tiled_matrix_multiply(
			std::get<0>(keypair), std::get<3>(keypair), problem_size, input_data_encrypt, output_data_encrypt
		);
	} else {
		std::cerr << "Unknown problem name" << std::endl;
		return 1;
	}

	std::chrono::high_resolution_clock::time_point calc_end = std::chrono::high_resolution_clock::now();
	std::cout << "Calc time: " << std::chrono::duration_cast<std::chrono::milliseconds>(calc_end - encrypt_end).count()
			  << " ms" << std::endl;

	std::vector<double> output_data;
	decrypt_file(std::get<0>(keypair), std::get<1>(keypair), output_data_encrypt, output_data);
	std::chrono::high_resolution_clock::time_point decrypt_end = std::chrono::high_resolution_clock::now();
	std::cout << "Decrypt time: "
			  << std::chrono::duration_cast<std::chrono::milliseconds>(decrypt_end - calc_end).count() << " ms"
			  << std::endl;

	std::cout << "Total time: "
			  << std::chrono::duration_cast<std::chrono::milliseconds>(decrypt_end - encrypt_start).count() << " ms"
			  << std::endl;

	write_to_file<bs>(output_file, output_data);

	return 0;
}
