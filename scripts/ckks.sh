#!/bin/bash

tool=
tool_args=

compile=
compile_flags=

input_size=
round_num=1
mem_limit=
workload=
thread_num=1

batch_size=1

current_time=$(date +%Y%m%d_%H%M%S)
current_date=$(date +%Y%m%d)

project_dir=$(realpath .)
log_dir=${project_dir}/logs/${current_date}/log_${current_time}
playground_dir=${project_dir}/logs/playground

tool_cmd=
log_file=/dev/null

OSPREY=${project_dir}/install/tools/osprey
MAGE=${project_dir}/install/tools/mage
PLANNER=${project_dir}/install/tools/planner
EXAMPLE_INPUT=${project_dir}/install/tools/example_input

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
		--thread_num=*)
			thread_num=$(echo $flag | awk -F = '{print $2}')
			;;
		*)
			echo "Unknown command-line flag" $flag
	esac
done

mkdir -p ${log_dir}

target_name=pckks_utils
problem_size=${input_size}
if [ "${thread_num}" == "1" ]; then
	target_name=ckks_utils
	thread_num=
	problem_size=${input_size}:${round_num}
fi
CKKS_UTILS=${project_dir}/install/tools/${target_name}

sync
echo 3 | tee /proc/sys/vm/drop_caches

log_file=${log_dir}/${workload}.log
touch ${log_file}
echo Tool: ${tool} | tee -a ${log_file}
echo Tool args: ${tool_args} | tee -a ${log_file}
echo Input size: ${input_size} | tee -a ${log_file}
echo Batch size: ${batch_size} | tee -a ${log_file}
echo Memory limit: ${mem_limit}M | tee -a ${log_file}
echo Thread number: ${thread_num} | tee -a ${log_file}

if [ "${tool}" == "osprey" ]; then
	tool_cmd="$OSPREY"
	tool_args="${tool_args} --trace-filebase=${workload}_${input_size}"
	target_name="ckks_utils"
elif [ "${tool}" == "mage" ]; then
	tool_cmd="$MAGE"
	tool_args=""
	target_name="mage"
elif [ "${tool}" == "baseline" ]; then
	tool_cmd=""
	tool_args=""
	target_name="ckks_utils"
else
	echo "Unknown tool" ${tool}
	exit
fi

rss_log_file=${log_dir}/${workload}_${input_size}.rss

touch ${rss_log_file}
echo "Timestamp,            RSS (MB)" > "$rss_log_file"

monitor_rss() {
	# Wait for the process to start
	while true; do
		PID=$(ps -a | grep "$target_name" | sort | tail -n 1 | awk '{print $1}')
		echo "PID: $PID"
		if [ -n "$PID" ]; then
			echo "Monitoring process $target_name with PID $PID"
			break
		fi
		sleep 1
	done
	
	# Start monitoring RSS
	while kill -0 $PID 2>/dev/null; do
		RSS=$(ps -o rss= -p $PID 2>/dev/null)
		if [ -z "$RSS" ]; then
			echo "Process $PID has terminated."
			break
		fi
		TIMESTAMP=$(date +"%Y-%m-%d %H:%M:%S")
		echo "$TIMESTAMP,  $(( RSS / 1024 ))" >> "$rss_log_file"
		sleep 1  # Adjust interval as needed
	done
}

if [ "${mem_limit}" != "" -a "${tool}" != "mage" ]; then
	cgcreate -g memory:/osprey
	cgset -r memory.high="${mem_limit}M" osprey
	tool_cmd="cgexec -g memory:osprey ${tool_cmd}"
fi

if [ "${compile}" == "true" ]; then
	pushd ${project_dir}
	make clean
	make tools -j16
	popd
fi

mkdir -p ${playground_dir}
pushd ${playground_dir}

$EXAMPLE_INPUT ${workload} ${input_size} 1 random

swapoff /dev/nvme0n1p2

monitor_rss &

echo expected output is $(od -An -t fD ${workload}_${input_size}_0.expected | tail -n 2)
if [ "${tool}" == "osprey" -o "${tool}" == "baseline" ]; then
	# echo 0 | $SUDO tee /sys/kernel/tracing/trace
	# echo nop | $SUDO tee /sys/kernel/tracing/current_tracer
	# echo 1 | $SUDO tee /sys/kernel/tracing/events/tlb/tlb_flush/enable
	# echo 1 | $SUDO tee /sys/kernel/tracing/tracing_on
	mkswap /dev/nvme0n1p2
	swapon /dev/nvme0n1p2
	${tool_cmd} ${tool_args} $CKKS_UTILS ${workload} ${problem_size} ${thread_num} ${workload}_${input_size}_0_garbler.input ${workload}_${input_size}_0_garbler.output 2>&1 | tee -a ${log_file}
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
          storage_path: /dev/nvme0n1p2
EOF
	for i in $(seq 1 $(( thread_num - 1 )) ); do
		cat <<EOF >>config.yaml
        - internal_host: localhost
          internal_port: $(( 56000 + i ))
          external_host: localhost
          external_port: $(( 57000 + i ))
          storage_path: /dev/nvme0n1p2
EOF
	done
	wait_pids=()
	$PLANNER ${workload} ckks config.yaml 0 0 ${input_size} | tee -a ${log_file}
	${tool_cmd} ckks config.yaml 0 0 ${workload}_${input_size} | tee -a ${log_file} &
	wait_pids+=($!)
	for i in $(seq 1 $(( thread_num - 1 )) ); do
		$PLANNER ${workload} ckks config.yaml 0 $i ${input_size} | tee -a ${log_file}
		${tool_cmd} ckks config.yaml 0 $i ${workload}_${input_size} | tee -a ${log_file} &
		wait_pids+=($!)
	done
	for pid in ${wait_pids[@]}; do
		wait $pid
	done
fi
echo real output is $(od -An -t fD ${workload}_${input_size}_0_garbler.output | tail -n 2)

popd

if [ "${mem_limit}" != "" -a "${tool}" != "mage" ]; then
	cgdelete memory:/osprey
fi
