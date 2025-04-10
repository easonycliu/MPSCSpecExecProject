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

#include "util.hpp"
#include "util/binaryfile.hpp"

double ckks_scale = std::pow(2.0, 40);

class Problem {
public:
	Problem() = default;
	virtual ~Problem() = default;
	virtual void divide(
		std::size_t workers, std::size_t problem_size, const std::vector<double>& input_data,
		std::vector<double>& share_input_data, std::vector<std::pair<std::size_t, std::vector<double>>>& divide_input_data
	) = 0;
	virtual void format(
		std::size_t problem_size, const std::vector<seal::Ciphertext>& share_input_data,
		const std::vector<seal::Ciphertext>& private_input_data,
		std::pair<
			std::vector<std::reference_wrapper<const seal::Ciphertext>>,
			std::vector<std::reference_wrapper<const seal::Ciphertext>>>& format_input_data
	) = 0;
	virtual void calculate(
		std::size_t problem_size, seal::EncryptionParameters& parms,
		const std::pair<
			std::vector<std::reference_wrapper<const seal::Ciphertext>>,
			std::vector<std::reference_wrapper<const seal::Ciphertext>>>& input_data,
		std::vector<seal::Ciphertext>& output_data
	) = 0;
	virtual void aggregate(const std::vector<std::vector<double>>& partial_output_data, std::vector<double>& output_data) = 0;
};

class VectorMultiply : public Problem {
public:
	VectorMultiply() = default;

	void divide(
		std::size_t workers, std::size_t problem_size, const std::vector<double>& input_data,
		std::vector<double>& share_input_data, std::vector<std::pair<std::size_t, std::vector<double>>>& divide_input_data
	) override {
		if (input_data.size() != problem_size * 2) {
			std::cerr << "Input data size does not match problem size." << std::endl;
			return;
		}
		share_input_data.clear();
		divide_input_data.clear();
		std::size_t vector_size_per_worker = problem_size / workers + std::size_t(problem_size % workers != 0);
		for (std::size_t i = 0; i < workers; ++i) {
			std::size_t start = i * vector_size_per_worker;
			std::size_t end = std::min(start + vector_size_per_worker, problem_size);
			std::vector<double> chunk(input_data.begin() + start, input_data.begin() + end);
			chunk.insert(
				chunk.end(), input_data.begin() + problem_size + start, input_data.begin() + problem_size + end
			);
			divide_input_data.emplace_back(end - start, std::move(chunk));
		}
	}

	void format(
		std::size_t problem_size, const std::vector<seal::Ciphertext>& share_input_data,
		const std::vector<seal::Ciphertext>& private_input_data,
		std::pair<
			std::vector<std::reference_wrapper<const seal::Ciphertext>>,
			std::vector<std::reference_wrapper<const seal::Ciphertext>>>& format_input_data
	) override {
		if (private_input_data.size() != problem_size * 2) {
			std::cerr << "Input data size must be twice the problem size." << std::endl;
			return;
		}

		for (std::size_t i = 0; i < problem_size; ++i) {
			format_input_data.first.emplace_back(private_input_data[i]);
			format_input_data.second.emplace_back(private_input_data[i + problem_size]);
		}
	}

	void calculate(
		std::size_t problem_size, seal::EncryptionParameters& parms,
		const std::pair<
			std::vector<std::reference_wrapper<const seal::Ciphertext>>,
			std::vector<std::reference_wrapper<const seal::Ciphertext>>>& input_data,
		std::vector<seal::Ciphertext>& output_data
	) override {
		if (input_data.first.size() != problem_size || input_data.second.size() != problem_size) {
			std::cerr << "Input data size must be twice the problem size." << std::endl;
			return;
		}
		if (input_data.first.size() == 0 || input_data.second.size() == 0) {
			return;
		}

		seal::SEALContext context(parms);
		seal::Evaluator evaluator(context);

		output_data.clear();
		output_data.resize(1);
		evaluator.multiply(
			input_data.first[0], input_data.second[0], output_data[0]
		);
		for (std::size_t i = 1; i < problem_size; ++i) {
			seal::Ciphertext temp;
			evaluator.multiply(input_data.first[i].get(), input_data.second[i].get(), temp);
			evaluator.add_inplace(output_data[0], temp);
		}
	}

	void aggregate(const std::vector<std::vector<double>>& partial_output_data, std::vector<double>& output_data) override {
		output_data.resize(1, 0);
		for (const std::vector<double>& one_partial_output_data : partial_output_data) {
			for (double item : one_partial_output_data) {
				output_data[0] += item;
			}
		}
	}
};

class MatrixVectorMultiply : public Problem {
public:
	MatrixVectorMultiply() = default;

	void divide(
		std::size_t workers, std::size_t problem_size, const std::vector<double>& input_data,
		std::vector<double>& share_input_data, std::vector<std::pair<std::size_t, std::vector<double>>>& divide_input_data
	) override {
		if (input_data.size() != problem_size * problem_size + problem_size) {
			std::cerr << "Input data size does not match problem size." << std::endl;
			return;
		}
		share_input_data.clear();
		for (std::size_t i = 0; i < problem_size; ++i) {
			share_input_data.push_back(input_data[i]);
		}
		divide_input_data.clear();
		std::size_t rows_per_worker = problem_size / workers + std::size_t(problem_size % workers != 0);
		for (std::size_t i = 0; i < workers; ++i) {
			std::size_t start = i * rows_per_worker * problem_size;
			std::size_t end = std::min(start + rows_per_worker * problem_size, problem_size * problem_size);
			if ((end - start) % problem_size != 0) {
				std::cerr << "Input data size does not match problem size." << std::endl;
				return;
			}
			std::vector<double> chunk(input_data.begin() + problem_size + start, input_data.begin() + problem_size + end);
			divide_input_data.emplace_back((end - start) / problem_size, std::move(chunk));
		}
	}

	void format(
		std::size_t problem_size, const std::vector<seal::Ciphertext>& share_input_data,
		const std::vector<seal::Ciphertext>& private_input_data,
		std::pair<
			std::vector<std::reference_wrapper<const seal::Ciphertext>>,
			std::vector<std::reference_wrapper<const seal::Ciphertext>>>& format_input_data
	) override {
		if (private_input_data.size() != problem_size * share_input_data.size()) {
			std::cerr << "Input data size does not match problem size." << std::endl;
			return;
		}
		for (std::size_t i = 0; i < private_input_data.size(); ++i) {
			format_input_data.first.emplace_back(private_input_data[i]);
		}
		for (std::size_t j = 0; j < share_input_data.size(); ++j) {
			format_input_data.second.emplace_back(share_input_data[j]);
		}
	}

	void calculate(
		std::size_t problem_size, seal::EncryptionParameters& parms,
		const std::pair<
			std::vector<std::reference_wrapper<const seal::Ciphertext>>,
			std::vector<std::reference_wrapper<const seal::Ciphertext>>>& input_data,
		std::vector<seal::Ciphertext>& output_data
	) override {
		if (input_data.first.size() != problem_size * input_data.second.size()) {
			std::cerr << "Input data size does not match problem size." << std::endl;
			return;
		}
		if (input_data.first.size() == 0 || input_data.second.size() == 0) {
			return;
		}

		seal::SEALContext context(parms);
		seal::Evaluator evaluator(context);

		output_data.clear();
		output_data.resize(problem_size);
		for (std::size_t i = 0; i < problem_size; ++i) {
			evaluator.multiply(
				input_data.first[i * input_data.second.size()], input_data.second[0], output_data[i]
			);
			for (std::size_t j = 1; j < input_data.second.size(); ++j) {
				seal::Ciphertext temp;
				evaluator.multiply(input_data.first[i * input_data.second.size() + j].get(), input_data.second[j].get(), temp);
				evaluator.add_inplace(output_data[i], temp);
			}
		}
	}

	void aggregate(const std::vector<std::vector<double>>& partial_output_data, std::vector<double>& output_data) override {
		output_data.clear();
		for (std::size_t i = 0; i < partial_output_data.size(); ++i) {
			output_data.insert(
				output_data.end(), partial_output_data[i].begin(), partial_output_data[i].end()
			);
		}
	}
};

class MatrixMultiply : public Problem {
public:
	MatrixMultiply() = default;

	void divide(
		std::size_t workers, std::size_t problem_size, const std::vector<double>& input_data,
		std::vector<double>& share_input_data, std::vector<std::pair<std::size_t, std::vector<double>>>& divide_input_data
	) override {
		if (input_data.size() != problem_size * problem_size * 2) {
			std::cerr << "Input data size does not match problem size." << std::endl;
			return;
		}
		share_input_data.clear();
		share_input_data.insert(share_input_data.end(), input_data.begin() + problem_size * problem_size, input_data.end());
		divide_input_data.clear();
		std::size_t rows_per_worker = problem_size / workers + std::size_t(problem_size % workers != 0);
		for (std::size_t i = 0; i < workers; ++i) {
			std::size_t start = i * rows_per_worker * problem_size;
			std::size_t end = std::min(start + rows_per_worker * problem_size, problem_size * problem_size);
			if ((end - start) % problem_size != 0) {
				std::cerr << "Input data size does not match problem size." << std::endl;
				return;
			}
			std::vector<double> chunk(input_data.begin() + start, input_data.begin() + end);
			divide_input_data.emplace_back((end - start) / problem_size, std::move(chunk));
		}
	}

	void format(
		std::size_t problem_size, const std::vector<seal::Ciphertext>& share_input_data,
		const std::vector<seal::Ciphertext>& private_input_data,
		std::pair<
			std::vector<std::reference_wrapper<const seal::Ciphertext>>,
			std::vector<std::reference_wrapper<const seal::Ciphertext>>>& format_input_data
	) override {
		std::size_t matrix_size = std::sqrt(share_input_data.size());
		if (share_input_data.size() != matrix_size * matrix_size) {
			std::cerr << "Input data of second operand size does not match problem size." << std::endl;
			return;
		}
		if (private_input_data.size() != problem_size * matrix_size) {
			std::cerr << "Input data of first operand size does not match problem size." << std::endl;
			return;
		}
		for (std::size_t i = 0; i < private_input_data.size(); ++i) {
			format_input_data.first.emplace_back(private_input_data[i]);
		}
		for (std::size_t j = 0; j < share_input_data.size(); ++j) {
			format_input_data.second.emplace_back(share_input_data[j]);
		}
	}

	void calculate(
		std::size_t problem_size, seal::EncryptionParameters& parms,
		const std::pair<
			std::vector<std::reference_wrapper<const seal::Ciphertext>>,
			std::vector<std::reference_wrapper<const seal::Ciphertext>>>& input_data,
		std::vector<seal::Ciphertext>& output_data
	) override {
		std::size_t matrix_size = std::sqrt(input_data.second.size());
		if (input_data.second.size() != matrix_size * matrix_size) {
			std::cerr << "Input data of second operand size does not match problem size." << std::endl;
			return;
		}
		if (input_data.first.size() != problem_size * matrix_size) {
			std::cerr << "Input data of first operand size does not match problem size." << std::endl;
			return;
		}
		if (input_data.first.size() == 0 || input_data.second.size() == 0) {
			return;
		}

		seal::SEALContext context(parms);
		seal::Evaluator evaluator(context);

		output_data.clear();
		output_data.resize(problem_size * matrix_size);
		for (std::size_t row_a = 0; row_a < problem_size; ++row_a) {
			for (std::size_t col_b = 0; col_b < matrix_size; ++col_b) {
				evaluator.multiply(
					input_data.first[row_a * matrix_size], input_data.second[col_b],
					output_data[row_a * matrix_size + col_b]
				);
				for (std::size_t i = 1; i < matrix_size; ++i) {
					seal::Ciphertext temp;
					evaluator.multiply(
						input_data.first[row_a * matrix_size + i], input_data.second[i * matrix_size + col_b], temp
					);
					evaluator.add_inplace(output_data[row_a * matrix_size + col_b], temp);
				}
			}
		}
	}

	void aggregate(const std::vector<std::vector<double>>& partial_output_data, std::vector<double>& output_data) override {
		output_data.clear();
		for (const std::vector<double>& one_partial_output_data : partial_output_data) {
			output_data.insert(
				output_data.end(), one_partial_output_data.begin(), one_partial_output_data.end()
			);
		}
	}
};

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
void to_ciphertext(
	std::shared_ptr<const seal::SEALContext::ContextData>& context_data, seal::Encryptor& encryptor,
	seal::CKKSEncoder& encoder, const T& input, seal::Ciphertext& output, std::size_t level
) {
	seal::parms_id_type target_level_parms_id = context_data->parms_id();

	seal::Plaintext plaintext;
	encoder.encode(input, target_level_parms_id, ckks_scale, plaintext);

	encryptor.encrypt(plaintext, output);
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

	output_data.clear();
	output_data.resize(input_data.size());
	for (seal::Ciphertext& item : output_data) {
		item.reserve(context, context_data->parms_id(), 2);
	}
	std::cerr << "Encrypting " << input_data.size() << " items" << std::endl;
	for (std::size_t i = 0; i != input_data.size(); i++) {
		to_ciphertext(context_data, encryptor, encoder, input_data[i], output_data[i], level);
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

int main(int argc, char** argv) {
	if (argc != 6) {
		std::cout << "Usage: " << argv[0] << " [problem_name] [problem_size] [thread_num] [input_file] [output_file]" << std::endl;
		return 1;
	}

	constexpr std::size_t bs = 4096;

	std::string problem_name = argv[1];
	std::size_t problem_size = std::stoull(argv[2]);
	std::size_t thread_num = std::stoull(argv[3]);
	std::string input_file = argv[4];
	std::string output_file = argv[5];

	std::size_t level;
	std::unique_ptr<Problem> problem;
	if (problem_name == "real_vector_multiply") {
		level = 1;
		problem = std::make_unique<VectorMultiply>();
	} else if (problem_name == "real_matrix_vector_multiply") {
		level = 1;
		problem = std::make_unique<MatrixVectorMultiply>();
	} else if (problem_name == "real_naive_matrix_multiply") {
		level = 1;
		problem = std::make_unique<MatrixMultiply>();
	} else {
		std::cerr << "Unknown problem name" << std::endl;
		return 1;
	}

	std::tuple<seal::EncryptionParameters, seal::SecretKey, seal::PublicKey, seal::RelinKeys, seal::GaloisKeys>
		keypair = keygen();

	std::vector<double> input_data;
	read_from_file<bs>(input_file, input_data);

	std::vector<std::pair<std::size_t, std::vector<double>>> divide_input_data;
	std::vector<double> share_input_data;
	problem->divide(
		thread_num, problem_size, input_data, share_input_data, divide_input_data
	);
	std::vector<std::vector<double>> partial_output_data(divide_input_data.size());

	std::chrono::time_point<std::chrono::high_resolution_clock> start_encrypt_share =
		std::chrono::high_resolution_clock::now();
	std::vector<seal::Ciphertext> encrypt_share_input_data;
	encrypt_file(
		std::get<0>(keypair), std::get<2>(keypair), share_input_data, encrypt_share_input_data, level
	);
	std::chrono::time_point<std::chrono::high_resolution_clock> end_encrypt_share =
		std::chrono::high_resolution_clock::now();
	std::cout << "Encrypt share time: "
			  << std::chrono::duration_cast<std::chrono::milliseconds>(end_encrypt_share - start_encrypt_share).count()
			  << " milliseconds" << std::endl;

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
			std::vector<seal::Ciphertext> encrypt_input_data;
			encrypt_file(std::get<0>(keypair), std::get<2>(keypair), divide_input_data[i].second, encrypt_input_data, level);
			progress[0].second[i] = true;

			std::pair<
				std::vector<std::reference_wrapper<const seal::Ciphertext>>,
				std::vector<std::reference_wrapper<const seal::Ciphertext>>>
				format_input_data;
			problem->format(
				divide_input_data[i].first, encrypt_share_input_data, encrypt_input_data, format_input_data
			);
			std::vector<seal::Ciphertext> encrypt_output_data;
			problem->calculate(
				divide_input_data[i].first, std::get<0>(keypair), format_input_data, encrypt_output_data
			);
			progress[1].second[i] = true;

			decrypt_file(
				std::get<0>(keypair), std::get<1>(keypair), encrypt_output_data, partial_output_data[i]
			);
			progress[2].second[i] = true;
		});
	}

	std::chrono::time_point<std::chrono::high_resolution_clock> total_start = std::chrono::high_resolution_clock::now();
	for (std::size_t i = 0; i < progress.size(); ++i) {
		std::chrono::time_point<std::chrono::high_resolution_clock> start = std::chrono::high_resolution_clock::now();
		double start_cpu_time = get_cpu_time_ms();
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
		double end_cpu_time = get_cpu_time_ms();
		std::chrono::time_point<std::chrono::high_resolution_clock> end = std::chrono::high_resolution_clock::now();
		std::cout << progress[i].first
				  << " cpu time: " << end_cpu_time - start_cpu_time << " milliseconds"
				  << std::endl;
		std::cout << progress[i].first
				  << " time: " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
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

	std::vector<double> output_data;
	problem->aggregate(partial_output_data, output_data);
	write_to_file<bs>(output_file, output_data);

	return 0;
}
