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
#include "util.hpp"

class Problem {
public:
	Problem() = default;
	virtual ~Problem() = default;
	virtual void divide(
		std::size_t workers, std::size_t problem_size, const std::vector<int>& input_data,
		std::vector<int>& share_input_data, std::vector<std::pair<std::size_t, std::vector<int>>>& divide_input_data
	) = 0;
	virtual void format(
		std::size_t problem_size, const std::vector<Ciphertext>& share_input_data,
		const std::vector<Ciphertext>& private_input_data,
		std::pair<
			std::vector<std::reference_wrapper<const Ciphertext>>,
			std::vector<std::reference_wrapper<const Ciphertext>>>& format_input_data
	) = 0;
	virtual void calculate(
		std::size_t problem_size, FHE_KeyPair& keypair,
		const std::pair<
			std::vector<std::reference_wrapper<const Ciphertext>>,
			std::vector<std::reference_wrapper<const Ciphertext>>>& input_data,
		std::vector<Ciphertext>& output_data
	) = 0;
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

	void divide(
		std::size_t workers, std::size_t problem_size, const std::vector<int>& input_data,
		std::vector<int>& share_input_data, std::vector<std::pair<std::size_t, std::vector<int>>>& divide_input_data
	) override {
		if (input_data.size() != problem_size * 2) {
			std::cerr << "Input data size must be twice the problem size." << std::endl;
			return;
		}
		share_input_data.clear();
		divide_input_data.clear();
		std::size_t vector_size_per_worker = problem_size / workers + std::size_t(problem_size % workers != 0);
		for (std::size_t i = 0; i < workers; ++i) {
			std::size_t start = i * vector_size_per_worker;
			std::size_t end = std::min(start + vector_size_per_worker, problem_size);
			std::vector<int> chunk(input_data.begin() + start, input_data.begin() + end);
			chunk.insert(
				chunk.end(), input_data.begin() + problem_size + start, input_data.begin() + problem_size + end
			);
			divide_input_data.emplace_back(end - start, std::move(chunk));
		}
	}

	void format(
		std::size_t problem_size, const std::vector<Ciphertext>& share_input_data,
		const std::vector<Ciphertext>& private_input_data,
		std::pair<
			std::vector<std::reference_wrapper<const Ciphertext>>,
			std::vector<std::reference_wrapper<const Ciphertext>>>& format_input_data
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
		std::size_t problem_size, FHE_KeyPair& keypair,
		const std::pair<
			std::vector<std::reference_wrapper<const Ciphertext>>,
			std::vector<std::reference_wrapper<const Ciphertext>>>& input_data,
		std::vector<Ciphertext>& output_data
	) override {
		if (input_data.first.size() != problem_size || input_data.second.size() != problem_size) {
			std::cerr << "Input data size must be twice the problem size." << std::endl;
			return;
		}

		output_data.resize(1, to_ciphertext(keypair, 0).mul(keypair.pk, to_ciphertext(keypair, 1)));
		for (std::size_t i = 0; i < problem_size; ++i) {
			output_data[0] += input_data.first[i].get().mul(keypair.pk, input_data.second[i].get());
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

class MatrixVectorMultiply : public Problem {
public:
	MatrixVectorMultiply() = default;

	void divide(
		std::size_t workers, std::size_t problem_size, const std::vector<int>& input_data,
		std::vector<int>& share_input_data, std::vector<std::pair<std::size_t, std::vector<int>>>& divide_input_data
	) override {
		if (input_data.size() != problem_size * problem_size + problem_size) {
			std::cerr << "Input data size does not match problem size." << std::endl;
			return;
		}
		share_input_data.clear();
		share_input_data.insert(share_input_data.end(), input_data.begin(), input_data.begin() + problem_size);
		divide_input_data.clear();
		std::size_t rows_per_worker = problem_size / workers + std::size_t(problem_size % workers != 0);
		for (std::size_t i = 0; i < workers; ++i) {
			std::size_t start = i * rows_per_worker * problem_size;
			std::size_t end = std::min(start + rows_per_worker * problem_size, problem_size * problem_size);
			if ((end - start) % problem_size != 0) {
				std::cerr << "Input data size does not match problem size." << std::endl;
				return;
			}
			std::vector<int> chunk(input_data.begin() + problem_size + start, input_data.begin() + problem_size + end);
			divide_input_data.emplace_back((end - start) / problem_size, std::move(chunk));
		}
	}

	void format(
		std::size_t problem_size, const std::vector<Ciphertext>& share_input_data,
		const std::vector<Ciphertext>& private_input_data,
		std::pair<
			std::vector<std::reference_wrapper<const Ciphertext>>,
			std::vector<std::reference_wrapper<const Ciphertext>>>& format_input_data
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
		std::size_t problem_size, FHE_KeyPair& keypair,
		const std::pair<
			std::vector<std::reference_wrapper<const Ciphertext>>,
			std::vector<std::reference_wrapper<const Ciphertext>>>& input_data,
		std::vector<Ciphertext>& output_data
	) override {
		if (input_data.first.size() != problem_size * input_data.second.size()) {
			std::cerr << "Input data size does not match problem size." << std::endl;
			return;
		}

		output_data.resize(problem_size, to_ciphertext(keypair, 0).mul(keypair.pk, to_ciphertext(keypair, 1)));
		for (std::size_t i = 0; i < problem_size; ++i) {
			for (std::size_t j = 0; j < input_data.second.size(); ++j) {
				output_data[i] += input_data.first[i * input_data.second.size() + j].get().mul(
					keypair.pk, input_data.second[j].get()
				);
			}
		}
	}

	void aggregate(const std::vector<std::vector<int>>& partial_output_data, std::vector<int>& output_data) override {
		output_data.clear();
		for (const std::vector<int>& one_partial_output_data : partial_output_data) {
			output_data.insert(output_data.end(), one_partial_output_data.begin(), one_partial_output_data.end());
		}
	}
};

class MatrixMultiply : public Problem {
public:
	MatrixMultiply() = default;

	void divide(
		std::size_t workers, std::size_t problem_size, const std::vector<int>& input_data,
		std::vector<int>& share_input_data, std::vector<std::pair<std::size_t, std::vector<int>>>& divide_input_data
	) override {
		if (input_data.size() != problem_size * problem_size * 2) {
			std::cerr << "Input data size does not match problem size." << std::endl;
			return;
		}
		share_input_data.clear();
		share_input_data.insert(
			share_input_data.end(), input_data.begin() + problem_size * problem_size, input_data.end()
		);
		divide_input_data.clear();
		std::size_t rows_per_worker = problem_size / workers + std::size_t(problem_size % workers != 0);
		for (std::size_t i = 0; i < workers; ++i) {
			std::size_t start = i * rows_per_worker * problem_size;
			std::size_t end = std::min(start + rows_per_worker * problem_size, problem_size * problem_size);
			if ((end - start) % problem_size != 0) {
				std::cerr << "Input data size does not match problem size." << std::endl;
				return;
			}
			std::vector<int> chunk(input_data.begin() + start, input_data.begin() + end);
			divide_input_data.emplace_back((end - start) / problem_size, std::move(chunk));
		}
	}

	void format(
		std::size_t problem_size, const std::vector<Ciphertext>& share_input_data,
		const std::vector<Ciphertext>& private_input_data,
		std::pair<
			std::vector<std::reference_wrapper<const Ciphertext>>,
			std::vector<std::reference_wrapper<const Ciphertext>>>& format_input_data
	) override {
		std::size_t matrix_size = std::sqrt(share_input_data.size());
		if (share_input_data.size() != matrix_size * matrix_size) {
			std::cerr << "Input data size does not match problem size." << std::endl;
			return;
		}
		if (private_input_data.size() != problem_size * matrix_size) {
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
		std::size_t problem_size, FHE_KeyPair& keypair,
		const std::pair<
			std::vector<std::reference_wrapper<const Ciphertext>>,
			std::vector<std::reference_wrapper<const Ciphertext>>>& input_data,
		std::vector<Ciphertext>& output_data
	) override {
		std::size_t matrix_size = std::sqrt(input_data.second.size());
		if (input_data.second.size() != matrix_size * matrix_size) {
			std::cerr << "Input data size does not match problem size." << std::endl;
			return;
		}
		if (input_data.first.size() != problem_size * matrix_size) {
			std::cerr << "Input data size does not match problem size." << std::endl;
			return;
		}

		output_data.resize(
			problem_size * matrix_size, to_ciphertext(keypair, 0).mul(keypair.pk, to_ciphertext(keypair, 1))
		);
		for (std::size_t i = 0; i < problem_size; ++i) {
			for (std::size_t j = 0; j < matrix_size; ++j) {
				for (std::size_t k = 0; k < matrix_size; ++k) {
					output_data[i * matrix_size + j] += input_data.first[i * matrix_size + k].get().mul(
						keypair.pk, input_data.second[k * matrix_size + j].get()
					);
				}
			}
		}
	}

	void aggregate(const std::vector<std::vector<int>>& partial_output_data, std::vector<int>& output_data) override {
		output_data.clear();
		for (const std::vector<int>& one_partial_output_data : partial_output_data) {
			output_data.insert(output_data.end(), one_partial_output_data.begin(), one_partial_output_data.end());
		}
	}
};

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

	if (strcmp(problem_name, "pmpspdz_vector_multiply") == 0 || strcmp(problem_name, "mpspdz_vector_multiply") == 0) {
		problem = std::make_unique<VectorMultiply>();
	} else if (strcmp(problem_name, "pmpspdz_matrix_vector_multiply") == 0 || strcmp(problem_name, "mpspdz_matrix_vector_multiply") == 0) {
		problem = std::make_unique<MatrixVectorMultiply>();
	} else if (strcmp(problem_name, "pmpspdz_matrix_multiply") == 0 || strcmp(problem_name, "mpspdz_matrix_multiply") == 0) {
		problem = std::make_unique<MatrixMultiply>();
	} else {
		std::cerr << "Unknown problem name: " << problem_name << std::endl;
		return 1;
	}

	std::vector<std::pair<std::size_t, std::vector<int>>> divide_input_data;
	std::vector<int> share_input_data;
	problem->divide(thread_num, problem_size, input_data, share_input_data, divide_input_data);
	std::vector<std::vector<int>> partial_output_data(divide_input_data.size());

	std::chrono::time_point<std::chrono::high_resolution_clock> start_encrypt_share =
		std::chrono::high_resolution_clock::now();
	std::vector<Ciphertext> encrypt_share_input_data;
	encrypt_file(keypair, share_input_data, encrypt_share_input_data);
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
			std::vector<Ciphertext> encrypt_input_data;
			encrypt_file(keypair, divide_input_data[i].second, encrypt_input_data);
			progress[0].second[i] = true;

			std::pair<
				std::vector<std::reference_wrapper<const Ciphertext>>,
				std::vector<std::reference_wrapper<const Ciphertext>>>
				format_input_data;
			problem->format(
				divide_input_data[i].first, encrypt_share_input_data, encrypt_input_data, format_input_data
			);
			std::vector<Ciphertext> encrypt_output_data;
			problem->calculate(divide_input_data[i].first, keypair, format_input_data, encrypt_output_data);
			progress[1].second[i] = true;

			decrypt_file(keypair, encrypt_output_data, partial_output_data[i]);
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

	std::vector<int> output_data;
	problem->aggregate(partial_output_data, output_data);
	write_to_file<bs>(output_file, output_data);

	return 0;
}
