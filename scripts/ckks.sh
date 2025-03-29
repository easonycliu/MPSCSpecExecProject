#!/bin/bash

tool=
tool_args=

compile=
compile_flags=

input_size=
round_num=
mem_limit=
workload=

batch_size=1

current_time=$(date +%Y%m%d_%H%M%S)
current_date=$(date +%Y%m%d)

project_dir=$(git rev-parse --show-toplevel)
log_dir=${project_dir}/logs/${current_date}/log_${current_time}
# playground_dir=${log_dir}

tool_cmd=
log_file=/dev/null

OSPREY=${project_dir}/install/tools/osprey
MAGE=${project_dir}/install/tools/mage
CKKS_UTILS=${project_dir}/install/tools/ckks_utils
PLANNER=${project_dir}/install/tools/planner
EXAMPLE_INPUT=${project_dir}/install/tools/example_input

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
		--workload=*)
			workload=$(echo $flag | awk -F = '{print $2}')
			;;
		*)
            echo "Unknown command-line flag" $flag
	esac
done

$SUDO sync
echo 3 | $SUDO tee /proc/sys/vm/drop_caches

log_file=${log_dir}/${workload}.log
mkdir -p ${log_dir}
touch ${log_file}
echo Tool: ${tool} | tee -a ${log_file}
echo Tool args: ${tool_args} | tee -a ${log_file}
echo Input size: ${input_size} | tee -a ${log_file}
echo Batch size: ${batch_size} | tee -a ${log_file}
echo Memory limit: ${mem_limit}M | tee -a ${log_file}

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

if [ "${mem_limit}" != "" ]; then
	$SUDO cgcreate -g memory:/osprey
	$SUDO cgset -r memory.high="${mem_limit}M" osprey
	tool_cmd="cgexec -g memory:osprey ${tool_cmd}"
fi

if [ "${compile}" == "true" ]; then
	pushd ${project_dir}
	make clean
	make tools -j16
	popd
fi

playground_dir=${project_dir}/logs/playground
mkdir -p ${playground_dir}
pushd ${playground_dir}

echo expected output is $(od -An -f ${workload}_${input_size}_0.expected | tail -n 2)
if [ "${tool}" == "osprey" -o "${tool}" == "baseline" ]; then
	# echo 0 | $SUDO tee /sys/kernel/tracing/trace
	# echo nop | $SUDO tee /sys/kernel/tracing/current_tracer
	# echo 1 | $SUDO tee /sys/kernel/tracing/events/tlb/tlb_flush/enable
	# echo 1 | $SUDO tee /sys/kernel/tracing/tracing_on
	$SUDO ${tool_cmd} \
		$CKKS_UTILS ${workload} $(( input_size / batch_size )) $round_num ${workload}_${input_size}_0_garbler.input ${workload}_${input_size}_0_garbler.output 2>&1 | tee -a ${log_file}
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
round_num: ${round_num}
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
	$SUDO $PLANNER ${workload} ckks config.yaml 0 0 ${input_size} | tee -a ${log_file}
	$SUDO ${tool_cmd} ckks config.yaml 0 0 ${workload}_${input_size} | tee -a ${log_file}
	$SUDO mv ${workload}_${input_size}_0.output ${workload}_${input_size}_0_garbler.output
fi
$CKKS_UTILS decrypt_file 1 ${workload}_${input_size}_0_garbler.output
echo real output is $(od -An -f ${workload}_${input_size}_0_garbler.output | tail -n 2)

popd

if [ "${mem_limit}" != "" ]; then
	$SUDO cgdelete memory:/osprey
fi
