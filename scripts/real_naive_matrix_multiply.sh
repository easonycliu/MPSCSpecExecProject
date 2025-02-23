#!/bin/bash

tool=
tool_args=

compile=
compile_flags=

input_size=
round_num=
mem_limit=
calculate_only=
enable_log=

batch_size=1

project_dir=$(realpath $(pwd)/..)
current_time=$(date +%Y%m%d_%H%M%S)
current_date=$(date +%Y%m%d)

lib_path=${project_dir}/install/oblivious-SEAL/lib:${project_dir}/install/tfhe/lib:${project_dir}/install/yaml-cpp/lib:${project_dir}/osprey/bin

tool_cmd=
log_file=/dev/null

PATH_SAVED=$PATH
if [ -d ${project_dir}/osprey/bin ]; then
	export PATH=${project_dir}/osprey/bin:$PATH
fi
if [ -d ${project_dir}/mage/bin ]; then
	export PATH=${project_dir}/mage/bin:$PATH
fi

OSPREY=$(which osprey)
MAGE=$(which mage)
CKKS_UTILS=$(which ckks_utils)
PLANNER=$(which planner)
EXAMPLE_INPUT=$(which example_input)

SUDO=
if [ $(id -u) -ne 0 ]; then
	SUDO=sudo
fi

for flag in "$@"; do
	case $flag in
		--tool=*)
			tool=$(echo $flag | awk -F = '{print $2}')
			;;
		--tool_args=*)
			value_index=$(( $(echo $flag | grep -bo = | awk -F : '{print $1}' | head -n 1) + 1 ))
			tool_args=$(echo ${flag:$value_index})
			;;
		--compile=*)
			compile=$(echo $flag | awk -F = '{print $2}')
			;;
		--compile_flags=*)
			value_index=$(( $(echo $flag | grep -bo = | awk -F : '{print $1}' | head -n 1) + 1 ))
			compile_flags=$(echo ${flag:$value_index})
			;;
		--input_size=*)
			input_size=$(echo $flag | awk -F = '{print $2}')
			;;
		--round_num=*)
			round_num=$(echo $flag | awk -F = '{print $2}')
			;;
		--mem_limit=*)
			mem_limit=$(echo $flag | awk -F = '{print $2}')
			;;
		--calculate_only)
			calculate_only=true
			;;
		--enable_log)
			enable_log=true
			;;
		*)
            echo "Unknown command-line flag" $flag
	esac
done

$SUDO sync
echo 3 | $SUDO tee /proc/sys/vm/drop_caches

if [ "${enable_log}" == "true" ]; then
	log_dir=${project_dir}/mage/logs/${current_date}/log_${current_time}
	log_file=${log_dir}/real_naive_matrix_multiply.log
	mkdir -p ${log_dir}
	touch ${log_file}
	echo Tool: ${tool} | tee -a ${log_file}
	echo Tool args: ${tool_args} | tee -a ${log_file}
	echo Input size: ${input_size} | tee -a ${log_file}
	echo Batch size: ${batch_size} | tee -a ${log_file}
	echo Memory limit: ${mem_limit}M | tee -a ${log_file}
fi

if [ "${tool}" == "osprey" ]; then
	tool_cmd="$OSPREY ${tool_args}"
elif [ "${tool}" == "mage" ]; then
	tool_cmd="$MAGE ${tool_args}"
elif [ "${tool}" == "baseline" ]; then
	tool_cmd=""
else
	echo "Unknown tool" ${tool}
	exit
fi

if [ -n "$(ldd bin/mage | grep "not found")" ]; then
	SUDO="$SUDO LD_LIBRARY_PATH=${lib_path}"
	export LD_LIBRARY_PATH=${lib_path}
fi

if [ "${mem_limit}" != "" ]; then
	$SUDO cgcreate -g memory:/osprey
	$SUDO cgset -r memory.high="${mem_limit}M" osprey
	tool_cmd="cgexec -g memory:osprey ${tool_cmd}"
fi

if [ "${compile}" == "true" ]; then
	make clean PROJECT_DIR=${project_dir}
	make $compile_flags PROJECT_DIR=${project_dir}
fi

if [ ! -d data ]; then
	mkdir data
fi
pushd data

if [ "${calculate_only}" != "true" ]; then
	$CKKS_UTILS keygen
	$EXAMPLE_INPUT real_naive_matrix_multiply ${input_size} 1 random
	$CKKS_UTILS encrypt_file ${batch_size} 1 real_naive_matrix_multiply_${input_size}_0_garbler.input
	echo size is ${input_size}
fi

echo expected output is $(od -An -f real_naive_matrix_multiply_${input_size}_0.expected | tail -n 2)
if [ "${tool}" == "osprey" -o "${tool}" == "baseline" ]; then
	# echo 0 | $SUDO tee /sys/kernel/tracing/trace
	# echo nop | $SUDO tee /sys/kernel/tracing/current_tracer
	# echo 1 | $SUDO tee /sys/kernel/tracing/events/tlb/tlb_flush/enable
	# echo 1 | $SUDO tee /sys/kernel/tracing/tracing_on
	$SUDO ${tool_cmd} \
		$CKKS_UTILS real_naive_matrix_multiply $(( input_size / batch_size )) $round_num real_naive_matrix_multiply_${input_size}_0_garbler.input real_naive_matrix_multiply_${input_size}_0_garbler.output 2>&1 | tee -a ${log_file}
	# echo 0 | $SUDO tee /sys/kernel/tracing/tracing_on
elif [ "${tool}" == "mage" ]; then
	page_shift=21
	num_pages=32768
	if [ "${mem_limit}" != "" ]; then
		num_pages=$(( ( ( mem_limit << 20 ) >> page_shift ) - 48 ))
	fi
	cat <<EOF >config.yaml
page_shift: ${page_shift}
num_pages: ${num_pages}
prefetch_buffer_size: 16
prefetch_lookahead: 100

parties:
    # Evaluator
    - workers:
        - internal_host: localhost
          internal_port: 56000
          external_host: localhost
          external_port: 57000
          storage_path: evaluator_swapfile_1
EOF
	$SUDO $PLANNER real_naive_matrix_multiply ckks config.yaml 0 0 ${input_size} | tee -a ${log_file}
	$SUDO ${tool_cmd} ckks config.yaml 0 0 real_naive_matrix_multiply_${input_size} | tee -a ${log_file}
	# $SUDO mv real_naive_matrix_multiply_${input_size}_0.output real_naive_matrix_multiply_${input_size}_0_garbler.output
fi
$CKKS_UTILS decrypt_file 1 real_naive_matrix_multiply_${input_size}_0_garbler.output
echo real output is $(od -An -f real_naive_matrix_multiply_${input_size}_0_garbler.output | tail -n 2)

popd

if [ "${mem_limit}" != "" ]; then
	$SUDO cgdelete memory:/osprey
fi

export PATH=$PATH_SAVED
