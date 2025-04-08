#ifndef MAPREDUCE_HPP
#define MAPREDUCE_HPP

#include <thread>
#include <vector>
#include <functional>

class MapReduce {
public:
	template <typename InputType, typename OutputType>
	static void
	map(const std::vector<InputType>& input_data, std::vector<OutputType>& mapped_data,
		std::function<OutputType(const InputType&)> map_func, std::size_t num_threads) {
		std::vector<std::thread> threads;

		const std::size_t num_elements_per_thread = input_data.size() / num_threads + int(input_data.size() % num_threads != 0);

		for (std::size_t t = 0; t < num_threads - 1; ++t) {
			threads.emplace_back([&, t]() {
				for (std::size_t i = t * num_elements_per_thread; i < std::min(input_data.size(), (t + 1) * num_elements_per_thread); ++i) {
					mapped_data[i] = map_func(input_data[i]);
				}
			});
		}

		for (std::size_t i = (num_threads - 1) * num_elements_per_thread; i < input_data.size(); ++i) {
			mapped_data[i] = map_func(input_data[i]);
		}

		for (auto& thread : threads) {
			thread.join();
		}
	}

	template <typename InputType, typename OutputType>
	static void
	map(const std::vector<InputType>& input_data, std::vector<OutputType>& mapped_data,
		std::function<void(const InputType&, OutputType&)> map_func, std::size_t num_threads) {
		std::vector<std::thread> threads;

		const std::size_t num_elements_per_thread = input_data.size() / num_threads + int(input_data.size() % num_threads != 0);

		for (std::size_t t = 0; t < num_threads - 1; ++t) {
			threads.emplace_back([&, t]() {
				for (std::size_t i = t * num_elements_per_thread; i < std::min(input_data.size(), (t + 1) * num_elements_per_thread); ++i) {
					map_func(input_data[i], mapped_data[i]);
				}
			});
		}

		for (std::size_t i = (num_threads - 1) * num_elements_per_thread; i < input_data.size(); ++i) {
			map_func(input_data[i], mapped_data[i]);
		}

		for (auto& thread : threads) {
			thread.join();
		}
	}

	template <typename OutputType>
	static void reduce(
		const std::vector<OutputType>& mapped_data, OutputType& result,
		std::function<OutputType(const OutputType&, const OutputType&)> reduce_func
	) {
		for (std::size_t i = 0; i < mapped_data.size(); i++) {
			result = reduce_func(result, mapped_data[i]);
		}
	}
};

#endif // MAPREDUCE_HPP
