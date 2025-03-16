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
#include <string>
#include <thread>

#include "util/binaryfile.hpp"

double ckks_scale = std::pow(2.0, 40);

void check_num_args(int argc, int expected) {
	if (argc != expected) {
		std::cerr << "Need " << expected << " arguments" << std::endl;
		std::abort();
	}
}

seal::EncryptionParameters parms_from_file(const char* filename) {
	seal::EncryptionParameters parms;
	std::ifstream parms_file(filename, std::ios::binary);
	parms.load(parms_file);
	return parms;
}

template <typename T>
T from_file(const seal::SEALContext& context, const char* filename) {
	T t;
	std::ifstream t_file(filename, std::ios::binary);
	t.load(context, t_file);
	return t;
}

int main(int argc, char** argv) {
	if (argc < 2) {
		std::cerr << "Usage: " << argv[0] << " [command]" << std::endl;
		return EXIT_FAILURE;
	}

	if (std::strcmp(argv[1], "keygen") == 0) {
		check_num_args(argc, 2);

		seal::EncryptionParameters parms(seal::scheme_type::ckks);
		size_t poly_modulus_degree = 8192;
		parms.set_poly_modulus_degree(poly_modulus_degree);
		parms.set_coeff_modulus(seal::CoeffModulus::Create(poly_modulus_degree, {60, 40, 40, 60}));
		{
			std::ofstream parms_file("parms.ckks", std::ios::binary);
			parms.save(parms_file);
		}

		seal::SEALContext context(parms);
		seal::KeyGenerator keygen(context);
		{
			auto secret_key = keygen.secret_key();
			std::ofstream sk_file("secretkey.ckks", std::ios::binary);
			secret_key.save(sk_file);
		}
		{
			seal::PublicKey public_key;
			keygen.create_public_key(public_key);
			std::ofstream pk_file("publickey.ckks", std::ios::binary);
			public_key.save(pk_file);
		}
		{
			seal::RelinKeys relin_keys;
			keygen.create_relin_keys(relin_keys);
			std::ofstream rk_file("relinkeys.ckks", std::ios::binary);
			relin_keys.save(rk_file);
		}
		{
			seal::GaloisKeys gal_keys;
			keygen.create_galois_keys(gal_keys);
			std::ofstream gk_file("galoiskeys.ckks", std::ios::binary);
			gal_keys.save(gk_file);
		}
	} else if (std::strcmp(argv[1], "encrypt_batch") == 0) {
		if (argc < 4) {
			std::cerr << "Usage: " << argv[0] << " encrypt_batch filename double1 [double2] ..." << std::endl;
			std::abort();
		}

		seal::EncryptionParameters parms = parms_from_file("parms.ckks");
		seal::SEALContext context(parms);
		seal::PublicKey public_key = from_file<seal::PublicKey>(context, "publickey.ckks");
		seal::Encryptor encryptor(context, public_key);

		std::vector<double> batch_data;
		for (int i = 3; i != argc; i++) {
			double item = std::stod(argv[i]);
			batch_data.push_back(item);
		}

		seal::Plaintext plaintext;
		seal::CKKSEncoder encoder(context);
		encoder.encode(batch_data, ckks_scale, plaintext);

		seal::Ciphertext ciphertext;
		encryptor.encrypt(plaintext, ciphertext);

		{
			std::cout << "Up to " << ciphertext.save_size() << " bytes" << std::endl;
			std::ofstream ciphertext_file(argv[2], std::ios::binary);
			ciphertext.save(ciphertext_file);
		}
	} else if (std::strcmp(argv[1], "encrypt_file") == 0) {
		if (argc < 3) {
			std::cerr << "Usage: " << argv[0] << " encrypt_file batch_size level [file1] ..." << std::endl;
			std::abort();
		}
		seal::EncryptionParameters parms = parms_from_file("parms.ckks");
		seal::SEALContext context(parms);
		seal::PublicKey public_key = from_file<seal::PublicKey>(context, "publickey.ckks");
		std::cout << "Slot count calculated is " << (*context.first_context_data()).parms().poly_modulus_degree()
				  << std::endl;
		seal::Encryptor encryptor(context, public_key);

		int batch_size = std::stoi(argv[2]);
		if (batch_size <= 0) {
			std::cerr << "Batch size must be positive" << std::endl;
			std::abort();
		}

		int level = std::stoi(argv[3]);
		if (level < 0) {
			std::cerr << "Level must be nonnegative" << std::endl;
			std::abort();
		}
		auto context_data = context.first_context_data();
		while (context_data->chain_index() > level) {
			context_data = context_data->next_context_data();
		}
		if (context_data->chain_index() != level) {
			std::cout << "Could not find params for level " << level << " (max level is "
					  << context.first_context_data()->chain_index() << ")" << std::endl;
			std::abort();
		}
		auto target_level_parms_id = context_data->parms_id();

		seal::CKKSEncoder encoder(context);

		for (int i = 4; i != argc; i++) {
			std::string filename(argv[i]);

			std::string temp_name = filename + ".plain";
			std::filesystem::rename(filename, temp_name);

			std::ofstream target(argv[i], std::ios::binary);

			std::vector<double> batch_data;

			mage::util::BinaryFileReader reader(temp_name.c_str());
			std::uint64_t num_bytes = reader.get_file_length();
			std::uint64_t num_uint32s = num_bytes >> 2;
			for (std::uint64_t i = 0; i != num_uint32s; i++) {
				double value = reader.BinaryReader::read<float>();
				batch_data.push_back(value);
				if (batch_data.size() == batch_size || i + 1 == num_uint32s) {
					seal::Plaintext plaintext;
					encoder.encode(batch_data, target_level_parms_id, ckks_scale, plaintext);

					seal::Ciphertext ciphertext;
					encryptor.encrypt(plaintext, ciphertext);
					ciphertext.save(target);

					batch_data.clear();
				}
			}
		}
	} else if (std::strcmp(argv[1], "decrypt_batch") == 0) {
		check_num_args(argc, 3);

		seal::EncryptionParameters parms = parms_from_file("parms.ckks");
		seal::SEALContext context(parms);
		seal::SecretKey secret_key = from_file<seal::SecretKey>(context, "secretkey.ckks");
		seal::Decryptor decryptor(context, secret_key);

		seal::Ciphertext ciphertext = from_file<seal::Ciphertext>(context, argv[2]);

		seal::Plaintext plaintext;
		decryptor.decrypt(ciphertext, plaintext);

		seal::CKKSEncoder encoder(context);
		std::vector<double> batch_data;
		encoder.decode(plaintext, batch_data);
		for (std::size_t i = 0; i != batch_data.size(); i++) {
			std::cout << batch_data[i];
			if (i + 1 == batch_data.size()) {
				std::cout << std::endl;
			} else {
				std::cout << " ";
			}
		}
	} else if (std::strcmp(argv[1], "decrypt_file") == 0) {
		if (argc < 3) {
			std::cerr << "Usage: " << argv[0] << " decrypt_file batch_size [file1] ..." << std::endl;
			std::abort();
		}

		seal::EncryptionParameters parms = parms_from_file("parms.ckks");
		seal::SEALContext context(parms);
		seal::SecretKey secret_key = from_file<seal::SecretKey>(context, "secretkey.ckks");
		seal::Decryptor decryptor(context, secret_key);

		int batch_size = std::stoi(argv[2]);
		if (batch_size <= 0) {
			std::cerr << "Batch size must be positive" << std::endl;
			std::abort();
		}

		seal::CKKSEncoder encoder(context);

		for (int i = 3; i != argc; i++) {
			std::string filename(argv[i]);

			std::string temp_name = filename + ".ciphertext";
			std::filesystem::rename(filename, temp_name);

			std::ifstream source(temp_name, std::ios::binary);

			std::vector<double> batch_data;

			mage::util::BinaryFileWriter writer(argv[i]);
			while (source.peek() != EOF) {
				seal::Ciphertext ciphertext;
				ciphertext.load(context, source);
				seal::Plaintext plaintext;
				decryptor.decrypt(ciphertext, plaintext);
				encoder.decode(plaintext, batch_data);
				for (std::size_t j = 0; j < batch_data.size() && j < batch_size; j++) {
					writer.write_float((float) batch_data[j]);
				}
				batch_data.clear();
			}
		}
	} else if (std::strcmp(argv[1], "switch") == 0) {
		check_num_args(argc, 4);

		seal::EncryptionParameters parms = parms_from_file("parms.ckks");
		seal::SEALContext context(parms);
		seal::Evaluator evaluator(context);

		seal::Ciphertext a = from_file<seal::Ciphertext>(context, argv[3]);
		seal::Ciphertext b;
		evaluator.mod_switch_to_next(a, b);
		b.scale() = ckks_scale; // the examples say to do this

		{
			std::cout << "Up to " << b.save_size() << " bytes" << std::endl;
			std::ofstream ciphertext_file(argv[2], std::ios::binary);
			b.save(ciphertext_file);
		}
	} else if (std::strcmp(argv[1], "add") == 0 || std::strcmp(argv[1], "multiply") == 0) {
		check_num_args(argc, 5);

		seal::EncryptionParameters parms = parms_from_file("parms.ckks");
		seal::SEALContext context(parms);
		seal::Evaluator evaluator(context);

		seal::Ciphertext a = from_file<seal::Ciphertext>(context, argv[3]);
		seal::Ciphertext b = from_file<seal::Ciphertext>(context, argv[4]);

		seal::Ciphertext c;
		if (std::strcmp(argv[1], "add") == 0) {
			evaluator.add(a, b, c);
		} else if (std::strcmp(argv[1], "multiply") == 0) {
			seal::RelinKeys relin_keys = from_file<seal::RelinKeys>(context, "relinkeys.ckks");
			evaluator.multiply(a, b, c);
			std::cout << "Nonlinear size: up to " << c.save_size() << " bytes" << std::endl;
			evaluator.relinearize_inplace(c, relin_keys);
			evaluator.rescale_to_next_inplace(c);
			c.scale() = ckks_scale; // the examples say to do this
		}

		{
			std::cout << "Up to " << c.save_size() << " bytes" << std::endl;
			std::ofstream ciphertext_file(argv[2], std::ios::binary);
			c.save(ciphertext_file);
		}
	} else if (std::strcmp(argv[1], "abpluscd") == 0) {
		check_num_args(argc, 8);

		seal::EncryptionParameters parms = parms_from_file("parms.ckks");
		seal::SEALContext context(parms);
		seal::Evaluator evaluator(context);

		seal::Ciphertext a = from_file<seal::Ciphertext>(context, argv[4]);
		seal::Ciphertext b = from_file<seal::Ciphertext>(context, argv[5]);
		seal::Ciphertext c = from_file<seal::Ciphertext>(context, argv[6]);
		seal::Ciphertext d = from_file<seal::Ciphertext>(context, argv[7]);

		seal::Ciphertext e;
		seal::RelinKeys relin_keys = from_file<seal::RelinKeys>(context, "relinkeys.ckks");
		std::chrono::time_point<std::chrono::steady_clock> start, end, add_start, add_end;
		{
			start = std::chrono::steady_clock::now();
			seal::Ciphertext temp1;
			seal::Ciphertext temp2;
			evaluator.multiply(a, b, temp1);
			evaluator.multiply(c, d, temp2);
			add_start = std::chrono::steady_clock::now();
			evaluator.add(temp1, temp2, e);
			add_end = std::chrono::steady_clock::now();
			evaluator.relinearize_inplace(e, relin_keys);
			evaluator.rescale_to_next_inplace(e);
			e.scale() = ckks_scale; // the examples say to do this
			end = std::chrono::steady_clock::now();
			std::chrono::microseconds us = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
			std::cerr << "Strategy 1: " << us.count() << " us" << std::endl;
			std::chrono::microseconds add_us =
				std::chrono::duration_cast<std::chrono::microseconds>(add_end - add_start);
			std::cerr << "Strategy 1 (add): " << add_us.count() << " us" << std::endl;
		}
		{
			std::cout << "Up to " << e.save_size() << " bytes" << std::endl;
			std::ofstream ciphertext_file(argv[2], std::ios::binary);
			e.save(ciphertext_file);
		}
		{
			start = std::chrono::steady_clock::now();
			seal::Ciphertext temp1;
			evaluator.multiply(a, b, temp1);
			evaluator.relinearize_inplace(temp1, relin_keys);
			evaluator.rescale_to_next_inplace(temp1);
			temp1.scale() = ckks_scale;

			seal::Ciphertext temp2;
			evaluator.multiply(c, d, temp2);
			evaluator.relinearize_inplace(temp2, relin_keys);
			evaluator.rescale_to_next_inplace(temp2);
			temp2.scale() = ckks_scale;
			end = std::chrono::steady_clock::now();
			std::chrono::microseconds us = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
			std::cerr << "Strategy 2: " << us.count() << " us" << std::endl;

			add_start = std::chrono::steady_clock::now();
			evaluator.add(temp1, temp2, e);
			add_end = std::chrono::steady_clock::now();
			std::chrono::microseconds add_us =
				std::chrono::duration_cast<std::chrono::microseconds>(add_end - add_start);
			std::cerr << "Strategy 2 (add): " << add_us.count() << " us" << std::endl;
		}
		{
			std::cout << "Up to " << e.save_size() << " bytes" << std::endl;
			std::ofstream ciphertext_file(argv[3], std::ios::binary);
			e.save(ciphertext_file);
		}
	} else if (std::strcmp(argv[1], "float_file_decode") == 0) {
		check_num_args(argc, 3);
		mage::util::BinaryFileReader reader(argv[2]);
		std::size_t total_length = reader.get_file_length();
		for (std::size_t amount_read = 0; amount_read < total_length; amount_read += 4) {
			float value = reader.BinaryReader::read<float>();
			std::cout << value << std::endl;
		}
	} else if (std::strcmp(argv[1], "real_statistics") == 0) {
		check_num_args(argc, 6);

		seal::EncryptionParameters parms = parms_from_file("parms.ckks");
		std::cout << "poly_modulus_degree is " << parms.poly_modulus_degree() << std::endl;
		std::cout << "coeff_modulus size is " << parms.coeff_modulus().size() << std::endl;
		seal::SEALContext context(parms);
		seal::Evaluator evaluator(context);

		seal::RelinKeys relin_keys = from_file<seal::RelinKeys>(context, "relinkeys.ckks");

		int problem_size = std::stoi(argv[2]);
		int round_num = std::stoi(argv[3]);
		std::ifstream input_file(argv[4], std::ios::binary);
		std::ofstream output_file(argv[5], std::ios::binary);

		auto start = std::chrono::steady_clock::now();

		std::cout << "Starting loading ciphertext" << std::endl;

		std::chrono::system_clock::time_point load_cph_start_time = std::chrono::system_clock::now();
		std::vector<seal::Ciphertext> input_points(problem_size);
		for (int i = 0; i != problem_size; i++) {
			input_points[i].load(context, input_file);
		}
		std::chrono::system_clock::time_point load_cph_end_time = std::chrono::system_clock::now();
		std::cout << "Finished loading ciphertext" << std::endl;
		std::cout
			<< "Time taken to load ciphertext is "
			<< std::chrono::duration_cast<std::chrono::milliseconds>(load_cph_end_time - load_cph_start_time).count()
			<< " ms" << std::endl;

		std::vector<seal::Ciphertext> squared_points(problem_size);
		seal::Ciphertext sum_squares;
		seal::Ciphertext temp_square;
		evaluator.square(input_points[0], sum_squares);
		evaluator.square(input_points[0], temp_square);
		seal::Ciphertext sum = input_points[0];
		size_t increased_size = 0;
		std::chrono::system_clock::time_point calc_start_time = std::chrono::system_clock::now();
		for (int i = 0; i != round_num; i++) {
			for (int j = 0; j != problem_size; j++) {
				evaluator.add_inplace(sum, input_points[j]);
				if (i == 0) {
					evaluator.square(input_points[j], squared_points[j]);
				}
				evaluator.add_inplace(sum_squares, squared_points[j]);
			}
		}
		std::chrono::system_clock::time_point calc_end_time = std::chrono::system_clock::now();
		evaluator.sub_inplace(sum_squares, temp_square);
		evaluator.sub_inplace(sum, input_points[0]);
		std::cout << "Finished calculating" << std::endl;
		std::cout << "Time taken to calculate is "
				  << std::chrono::duration_cast<std::chrono::milliseconds>(calc_end_time - calc_start_time).count()
				  << " ms" << std::endl;
		evaluator.relinearize_inplace(sum_squares, relin_keys);
		evaluator.rescale_to_next_inplace(sum_squares);
		sum_squares.scale() = ckks_scale;

		std::cout << "Slot count calculated is " << (*context.first_context_data()).parms().poly_modulus_degree()
				  << std::endl;
		seal::CKKSEncoder encoder(context);
		std::cout << "Slot count is " << encoder.slot_count() << std::endl;
		{
			auto context_data = context.first_context_data()->next_context_data();
			if (context_data->chain_index() != 1) {
				std::abort();
			}
			seal::Plaintext reciprocal_size;
			encoder.encode(
				1 / static_cast<double>(problem_size), context_data->parms_id(), ckks_scale, reciprocal_size
			);
			evaluator.multiply_plain_inplace(sum_squares, reciprocal_size);
			evaluator.relinearize_inplace(sum_squares, relin_keys);
			evaluator.rescale_to_next_inplace(sum_squares);
			sum_squares.scale() = ckks_scale;
		}
		{
			seal::Plaintext reciprocal_size;
			encoder.encode(1 / static_cast<double>(problem_size), ckks_scale, reciprocal_size);
			evaluator.multiply_plain_inplace(sum, reciprocal_size);
			evaluator.relinearize_inplace(sum, relin_keys);
			evaluator.rescale_to_next_inplace(sum);
			sum.scale() = ckks_scale;
		}

		seal::Ciphertext& mean = sum;
		seal::Ciphertext mean_squared;
		evaluator.square(mean, mean_squared);
		evaluator.relinearize_inplace(mean_squared, relin_keys);
		evaluator.rescale_to_next_inplace(mean_squared);
		mean_squared.scale() = ckks_scale;
		evaluator.sub_inplace(sum_squares, mean_squared);
		seal::Ciphertext& variance = sum_squares;

		mean.save(output_file);
		variance.save(output_file);

		auto end = std::chrono::steady_clock::now();
		std::chrono::milliseconds latency_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

		std::cout << latency_ms.count() << " ms" << std::endl;
	} else if (std::strcmp(argv[1], "real_sum") == 0) {
		check_num_args(argc, 6);

		seal::EncryptionParameters parms = parms_from_file("parms.ckks");
		seal::SEALContext context(parms);
		seal::Evaluator evaluator(context);

		seal::RelinKeys relin_keys = from_file<seal::RelinKeys>(context, "relinkeys.ckks");

		int problem_size = std::stoi(argv[2]);
		int round_num = std::stoi(argv[3]);
		std::ifstream input_file(argv[4], std::ios::binary);
		std::ofstream output_file(argv[5], std::ios::binary);

		auto start = std::chrono::steady_clock::now();

		std::cout << "Starting loading ciphertext" << std::endl;

		std::chrono::system_clock::time_point load_cph_start_time = std::chrono::system_clock::now();
		std::vector<seal::Ciphertext> input_points(problem_size);
		for (int i = 0; i != problem_size; i++) {
			input_points[i].load(context, input_file);
		}
		std::chrono::system_clock::time_point load_cph_end_time = std::chrono::system_clock::now();
		std::cout << "Finished loading ciphertext" << std::endl;
		std::cout
			<< "Time taken to load ciphertext is "
			<< std::chrono::duration_cast<std::chrono::milliseconds>(load_cph_end_time - load_cph_start_time).count()
			<< " ms" << std::endl;

		std::chrono::system_clock::time_point calc_start_time = std::chrono::system_clock::now();
		seal::Ciphertext sum = input_points[0];
		for (int i = 0; i != round_num; i++) {
			for (int j = 0; j != problem_size; j++) {
				evaluator.add_inplace(sum, input_points[j]);
			}
		}

		// for (int i = 1; i != problem_size; i++) {
		// 	evaluator.add_inplace(sum, input_points[i]);
		// }
		std::chrono::system_clock::time_point calc_end_time = std::chrono::system_clock::now();
		std::cout << "Finished calculating" << std::endl;
		std::cout << "Time taken to calculate is "
				  << std::chrono::duration_cast<std::chrono::milliseconds>(calc_end_time - calc_start_time).count()
				  << " ms" << std::endl;

		std::cout << "Slot count calculated is " << (*context.first_context_data()).parms().poly_modulus_degree()
				  << std::endl;
		seal::CKKSEncoder encoder(context);
		std::cout << "Slot count is " << encoder.slot_count() << std::endl;

		sum.save(output_file);

		auto end = std::chrono::steady_clock::now();
		std::chrono::milliseconds latency_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

		std::cout << latency_ms.count() << " ms" << std::endl;
	} else if (std::strcmp(argv[1], "real_naive_matrix_multiply") == 0) {
		check_num_args(argc, 5);

		seal::EncryptionParameters parms = parms_from_file("parms.ckks");
		seal::SEALContext context(parms);
		seal::Evaluator evaluator(context);

		seal::RelinKeys relin_keys = from_file<seal::RelinKeys>(context, "relinkeys.ckks");

		int problem_size = std::stoi(argv[2]);
		std::ifstream input_file(argv[3], std::ios::binary);
		std::ofstream output_file(argv[4], std::ios::binary);

		auto start = std::chrono::steady_clock::now();

		std::cout << "Starting loading ciphertext" << std::endl;

		std::chrono::system_clock::time_point load_cph_start_time = std::chrono::system_clock::now();
		std::vector<seal::Ciphertext> input_points_a(problem_size * problem_size);
		std::vector<seal::Ciphertext> input_points_b(problem_size * problem_size);
		for (int i = 0; i != problem_size * problem_size; i++) {
			input_points_a[i].load(context, input_file);
		}
		for (int i = 0; i != problem_size * problem_size; i++) {
			input_points_b[i].load(context, input_file);
		}
		std::chrono::system_clock::time_point load_cph_end_time = std::chrono::system_clock::now();

		std::cout << "Finished loading ciphertext" << std::endl;
		std::cout
			<< "Time taken to load ciphertext is "
			<< std::chrono::duration_cast<std::chrono::milliseconds>(load_cph_end_time - load_cph_start_time).count()
			<< " ms" << std::endl;

		std::vector<seal::Ciphertext> result(problem_size * problem_size);

		std::chrono::system_clock::time_point calc_start_time = std::chrono::system_clock::now();
		for (int row_a = 0; row_a != problem_size; row_a++) {
			for (int col_b = 0; col_b != problem_size; col_b++) {
				evaluator.multiply(
					input_points_a[row_a * problem_size], input_points_b[col_b * problem_size],
					result[row_a * problem_size + col_b]
				);
				for (int i = 1; i != problem_size; i++) {
					seal::Ciphertext temp;
					evaluator.multiply(
						input_points_a[row_a * problem_size + i], input_points_b[col_b * problem_size + i], temp
					);
					evaluator.add_inplace(result[row_a * problem_size + col_b], temp);
				}
			}
		}
		std::chrono::system_clock::time_point calc_end_time = std::chrono::system_clock::now();

		std::cout << "Finished calculating" << std::endl;
		std::cout << "Time taken to calculate is "
				  << std::chrono::duration_cast<std::chrono::milliseconds>(calc_end_time - calc_start_time).count()
				  << " ms" << std::endl;

		std::cout << "Slot count calculated is " << (*context.first_context_data()).parms().poly_modulus_degree()
				  << std::endl;
		seal::CKKSEncoder encoder(context);
		std::cout << "Slot count is " << encoder.slot_count() << std::endl;

		for (int i = 0; i != problem_size * problem_size; i++) {
			result[i].save(output_file);
		}

		auto end = std::chrono::steady_clock::now();
		std::chrono::milliseconds latency_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

		std::cout << latency_ms.count() << " ms" << std::endl;
	} else if (std::strcmp(argv[1], "real_matrix_vector_multiply") == 0) {
		check_num_args(argc, 5);

		seal::EncryptionParameters parms = parms_from_file("parms.ckks");
		seal::SEALContext context(parms);
		seal::Evaluator evaluator(context);

		seal::RelinKeys relin_keys = from_file<seal::RelinKeys>(context, "relinkeys.ckks");

		int problem_size = std::stoi(argv[2]);
		std::ifstream input_file(argv[3], std::ios::binary);
		std::ofstream output_file(argv[4], std::ios::binary);

		auto start = std::chrono::steady_clock::now();

		std::cout << "Starting loading ciphertext" << std::endl;

		std::chrono::system_clock::time_point load_cph_start_time = std::chrono::system_clock::now();
		std::vector<seal::Ciphertext> input_points_vector(problem_size);
		std::vector<seal::Ciphertext> input_points_matrix(problem_size * problem_size);
		for (int i = 0; i != problem_size; i++) {
			input_points_vector[i].load(context, input_file);
		}
		for (int i = 0; i != problem_size * problem_size; i++) {
			input_points_matrix[i].load(context, input_file);
		}
		std::chrono::system_clock::time_point load_cph_end_time = std::chrono::system_clock::now();

		std::cout << "Finished loading ciphertext" << std::endl;
		std::cout
			<< "Time taken to load ciphertext is "
			<< std::chrono::duration_cast<std::chrono::milliseconds>(load_cph_end_time - load_cph_start_time).count()
			<< " ms" << std::endl;

		std::vector<seal::Ciphertext> result(problem_size * problem_size);

		std::chrono::system_clock::time_point calc_start_time = std::chrono::system_clock::now();
		for (int row_matrix = 0; row_matrix != problem_size; row_matrix++) {
			evaluator.multiply(
				input_points_matrix[row_matrix * problem_size], input_points_vector[0], result[row_matrix]
			);
			for (int i = 1; i != problem_size; i++) {
				seal::Ciphertext temp;
				evaluator.multiply(input_points_matrix[row_matrix * problem_size + i], input_points_vector[i], temp);
				evaluator.add_inplace(result[row_matrix], temp);
			}
		}
		std::chrono::system_clock::time_point calc_end_time = std::chrono::system_clock::now();

		std::cout << "Finished calculating" << std::endl;
		std::cout << "Time taken to calculate is "
				  << std::chrono::duration_cast<std::chrono::milliseconds>(calc_end_time - calc_start_time).count()
				  << " ms" << std::endl;

		std::cout << "Slot count calculated is " << (*context.first_context_data()).parms().poly_modulus_degree()
				  << std::endl;
		seal::CKKSEncoder encoder(context);
		std::cout << "Slot count is " << encoder.slot_count() << std::endl;

		for (int i = 0; i != problem_size; i++) {
			result[i].save(output_file);
		}

		auto end = std::chrono::steady_clock::now();
		std::chrono::milliseconds latency_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

		std::cout << latency_ms.count() << " ms" << std::endl;
	} else if (std::strcmp(argv[1], "real_tiled_matrix_multiply") == 0) {
		if (argc != 5 && argc != 6) {
			std::cerr << "Usage: " << argv[0]
					  << " real_tiled_matrix_multiply problem_size input_file output_file tile_size" << std::endl;
			std::abort();
		}

		seal::EncryptionParameters parms = parms_from_file("parms.ckks");
		seal::SEALContext context(parms);
		seal::Evaluator evaluator(context);

		seal::RelinKeys relin_keys = from_file<seal::RelinKeys>(context, "relinkeys.ckks");

		std::size_t memory_size =
			std::getenv("OSPREY_MEM_LIMIT_HIGH") ? std::stoull(std::getenv("OSPREY_MEM_LIMIT_HIGH")) : 0;

		std::size_t problem_size = static_cast<std::size_t>(std::stoi(argv[2]));
		std::ifstream input_file(argv[3], std::ios::binary);
		std::ofstream output_file(argv[4], std::ios::binary);
		std::size_t tile_size = (argc == 6) ? static_cast<std::size_t>(std::stoi(argv[5])) : ((memory_size / 2048) + 1);

		std::cout << "Set tile size to " << tile_size << std::endl;

		auto start = std::chrono::steady_clock::now();

		std::cout << "Starting loading ciphertext" << std::endl;

		std::chrono::system_clock::time_point load_cph_start_time = std::chrono::system_clock::now();
		std::vector<seal::Ciphertext> input_points_a(problem_size * problem_size);
		std::vector<seal::Ciphertext> input_points_b(problem_size * problem_size);
		for (int i = 0; i != problem_size * problem_size; i++) {
			input_points_a[i].load(context, input_file);
		}
		for (int i = 0; i != problem_size * problem_size; i++) {
			input_points_b[i].load(context, input_file);
		}
		std::chrono::system_clock::time_point load_cph_end_time = std::chrono::system_clock::now();

		std::cout << "Finished loading ciphertext" << std::endl;
		std::cout
			<< "Time taken to load ciphertext is "
			<< std::chrono::duration_cast<std::chrono::milliseconds>(load_cph_end_time - load_cph_start_time).count()
			<< " ms" << std::endl;

		std::vector<seal::Ciphertext> result(problem_size * problem_size);

		std::chrono::system_clock::time_point calc_start_time = std::chrono::system_clock::now();
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
							if (result[row_a * problem_size + col_b].size() != 0) {
								evaluator.add_inplace(result[row_a * problem_size + col_b], dot_product_result);
							} else {
								result[row_a * problem_size + col_b] = std::move(dot_product_result);
							}
						}
					}
				}
			}
		}
		std::chrono::system_clock::time_point calc_end_time = std::chrono::system_clock::now();

		std::cout << "Finished calculating" << std::endl;
		std::cout << "Time taken to calculate is "
				  << std::chrono::duration_cast<std::chrono::milliseconds>(calc_end_time - calc_start_time).count()
				  << " ms" << std::endl;

		std::cout << "Slot count calculated is " << (*context.first_context_data()).parms().poly_modulus_degree()
				  << std::endl;
		seal::CKKSEncoder encoder(context);
		std::cout << "Slot count is " << encoder.slot_count() << std::endl;

		for (int i = 0; i != problem_size * problem_size; i++) {
			result[i].save(output_file);
		}

		auto end = std::chrono::steady_clock::now();
		std::chrono::milliseconds latency_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

		std::cout << latency_ms.count() << " ms" << std::endl;
	} else {
		std::cerr << "Unknown command " << argv[1] << std::endl;
		std::abort();
	}
}
