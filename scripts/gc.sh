#!/bin/bash

tool=
tool_args=

compile=

input_size=
mem_limit=
workload=
this_ip=
other_ip=

current_time=$(date +%Y%m%d_%H%M%S)
current_date=$(date +%Y%m%d)

project_dir=$(realpath .)
log_dir=${project_dir}/logs/${current_date}/log_${current_time}
playground_dir=${project_dir}/logs/playground

tool_cmd=
tool_args_garbler=
tool_args_evaluator=
log_file=/dev/null

OSPREY=${project_dir}/install/tools/osprey
EMP_UTILS=${project_dir}/install/tools/emp_utils
EXAMPLE_INPUT=${project_dir}/build/emp-tool/example_input

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
		--input_size=*)
			input_size=$(echo $flag | awk -F = '{print $2}')
			;;
		--mem_limit=*)
			mem_limit=$(echo $flag | awk -F = '{print $2}')
			;;
		--workload=*)
			workload=$(echo $flag | awk -F = '{print $2}')
			;;
		--this_ip=*)
			this_ip=$(echo $flag | awk -F = '{print $2}')
			;;
		--other_ip=*)
			other_ip=$(echo $flag | awk -F = '{print $2}')
			;;
		*)
			echo "Unknown command-line flag" $flag
	esac
done

sync
echo 3 | tee /proc/sys/vm/drop_caches

log_file=${log_dir}/${workload}.log
mkdir -p ${log_dir}
touch ${log_file}
echo Tool: ${tool} | tee -a ${log_file}
echo Tool args: ${tool_args} | tee -a ${log_file}
echo Input size: ${input_size} | tee -a ${log_file}
echo Memory limit: ${mem_limit}M | tee -a ${log_file}

if [ "${tool}" == "osprey" ]; then
	tool_cmd="$OSPREY"
	tool_args_garbler="${tool_args} --trace-filebase=${workload}_${input_size}_garbler"
	tool_args_evaluator="${tool_args} --trace-filebase=${workload}_${input_size}_evaluator"
elif [ "${tool}" == "baseline" ]; then
	tool_cmd=""
	tool_args_garbler=""
	tool_args_evaluator=""
else
	echo "Unknown tool" ${tool}
	exit
fi

rss_log_file=${log_dir}/${workload}_${input_size}.rss
target_name="emp_utils"

touch ${rss_log_file}
echo "Timestamp,            RSS (MB)" > "$rss_log_file"

monitor_rss() {
	# Wait for the process to start
	while true; do
		PID=$(ps -a | grep -E "$target_name*" | sort | tail -n 1 | awk '{print $1}')
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

if [ "${mem_limit}" != "" ]; then
	cgcreate -g memory:/osprey
	ssh -i /home/easonliu/.ssh/id_ed25519 easonliu@${other_ip} "fastsudo cgcreate -g memory:/osprey"
	cgset -r memory.high="${mem_limit}M" osprey
	ssh -i /home/easonliu/.ssh/id_ed25519 easonliu@${other_ip} "fastsudo cgset -r memory.high=${mem_limit}M osprey"
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

${EXAMPLE_INPUT} ${workload} ${input_size} 1
ssh -i /home/easonliu/.ssh/id_ed25519 easonliu@${other_ip} "cd ${playground_dir}; ${EXAMPLE_INPUT} ${workload} ${input_size} 1"

swapoff /dev/nvme0n1p2
ssh -i /home/easonliu/.ssh/id_ed25519 easonliu@${other_ip} "fastsudo swapoff /dev/nvme0n1p2"

echo expected output is $(od -An -w -i ${workload}_${input_size}_0.expected | tail -n 2)

mkswap /dev/nvme0n1p2
ssh -i /home/easonliu/.ssh/id_ed25519 easonliu@${other_ip} "fastsudo mkswap /dev/nvme0n1p2"
swapon /dev/nvme0n1p2
ssh -i /home/easonliu/.ssh/id_ed25519 easonliu@${other_ip} "fastsudo swapon /dev/nvme0n1p2"

echo ${tool_cmd} ${tool_args_garbler} ${EMP_UTILS}_garbler ${workload} ${input_size} 1234 127.0.0.1 ${workload}_${input_size}_garbler.input ${workload}_${input_size}_0_garbler.output
${tool_cmd} ${tool_args_garbler} ${EMP_UTILS}_garbler ${workload} ${input_size} 1234 127.0.0.1 ${workload}_${input_size}_garbler.input ${workload}_${input_size}_0_garbler.output 2>&1 | tee -a ${log_file}.garbler &

monitor_rss &

sleep 1
echo ssh -i /home/easonliu/.ssh/id_ed25519 easonliu@${other_ip} "cd ${playground_dir} && fastsudo ${tool_cmd} ${tool_args_evaluator} ${EMP_UTILS}_evaluator ${workload} ${input_size} 1234 ${this_ip} ${playground_dir}/${workload}_${input_size}_evaluator.input ${playground_dir}/${workload}_${input_size}_0_evaluator.output"
ssh -i /home/easonliu/.ssh/id_ed25519 easonliu@${other_ip} "cd ${playground_dir} && fastsudo ${tool_cmd} ${tool_args_evaluator} ${EMP_UTILS}_evaluator ${workload} ${input_size} 1234 ${this_ip} ${playground_dir}/${workload}_${input_size}_evaluator.input ${playground_dir}/${workload}_${input_size}_0_evaluator.output" | tee -a ${log_file}.evaluator

echo real output is $(od -An -w -i ${workload}_${input_size}_0_garbler.output | tail -n 2)

popd

if [ "${mem_limit}" != "" ]; then
	cgdelete memory:/osprey
	ssh -i /home/easonliu/.ssh/id_ed25519 easonliu@${other_ip} "fastsudo cgdelete memory:/osprey"
fi

stty sane
echo ""
