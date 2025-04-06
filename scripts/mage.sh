#!/bin/bash

tool=
tool_args=

input_size=
mem_limit=
workload=
this_ip=
other_ip=

batch_size=1

current_time=$(date +%Y%m%d_%H%M%S)
current_date=$(date +%Y%m%d)

project_dir=$(realpath .)
log_dir=${project_dir}/logs/${current_date}/log_${current_time}
playground_dir=${project_dir}/logs/playground

echo $project_dir

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
echo Batch size: ${batch_size} | tee -a ${log_file}
echo Memory limit: ${mem_limit}M | tee -a ${log_file}

page_shift=17
num_pages=1048576
tool_args_garbler=${tool_args}
tool_args_evaluator=${tool_args}
if [ "${tool}" == "osprey" ]; then
	tool_cmd="$OSPREY"
	tool_args_garbler="${tool_args} --trace-filebase=${workload}_${input_size}_garbler"
	tool_args_evaluator="${tool_args} --trace-filebase=${workload}_${input_size}_evaluator"
	mem_limit=""
elif [ "${tool}" == "mage" ]; then
	tool_cmd=""
	if [ "${mem_limit}" == "" ]; then
		echo "Error: Memory limit is required for MAGE"
		exit
	fi
	num_pages=$(( ( ( mem_limit << 16 ) >> page_shift ) - 48 ))
elif [ "${tool}" == "baseline" ]; then
	tool_cmd=""
	tool_args_garbler=""
	tool_args_evaluator=""
else
	echo "Unknown tool" ${tool}
	exit
fi

rss_log_file=${log_dir}/${workload}_${input_size}.rss
target_name="mage"

touch ${rss_log_file}
echo "Timestamp,            RSS (MB)" > "$rss_log_file"

monitor_rss() {
	# Wait for the process to start
	while true; do
		PID=$(ps -a | grep "${target_name}\$" | sort | head -n 1 | awk '{print $1}')
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
	ssh -i /home/yicheng/.ssh/id_rsa yicheng@${other_ip} "fastsudo cgcreate -g memory:/osprey"
	cgset -r memory.high="${mem_limit}M" osprey
	ssh -i /home/yicheng/.ssh/id_rsa yicheng@${other_ip} "fastsudo cgset -r memory.high=${mem_limit}M osprey"
	tool_cmd="cgexec -g memory:osprey ${tool_cmd}"
fi

mkdir -p ${playground_dir}
pushd ${playground_dir}

cat <<EOF >${playground_dir}/config.yaml
page_shift: ${page_shift}
num_pages: ${num_pages}
prefetch_buffer_size: 16
prefetch_lookahead: 100

parties:
    # Evaluator
    - workers:
        - internal_host: localhost
          internal_port: 56000
          external_host: ${other_ip}
          external_port: 54323
          storage_path: /dev/sdb2

    # Garbler
    - workers:
        - internal_host: localhost
          internal_port: 50000
          external_host: ${this_ip}
          external_port: 54322
          storage_path: /dev/sdb2
EOF

swapoff /dev/sdb2
ssh -i /home/yicheng/.ssh/id_rsa yicheng@${other_ip} "fastsudo swapoff /dev/sdb2"

scp -i /home/yicheng/.ssh/id_rsa ${playground_dir}/config.yaml yicheng@${other_ip}:${playground_dir}/config.yaml

${EXAMPLE_INPUT} ${workload} ${input_size} 1
ssh -i /home/yicheng/.ssh/id_rsa yicheng@${other_ip} "cd ${playground_dir}; ${EXAMPLE_INPUT} ${workload} ${input_size} 1"

echo expected output is $(od -An -w -i ${workload}_${input_size}_0.expected | tail -n 2)

$PLANNER ${workload} halfgates ${playground_dir}/config.yaml 0 0 ${input_size} | tee -a ${log_file}.evaluator
ssh -i /home/yicheng/.ssh/id_rsa yicheng@${other_ip} "cd ${playground_dir}; $PLANNER ${workload} halfgates ${playground_dir}/config.yaml 0 0 ${input_size}" | tee -a ${log_file}.garbler

monitor_rss &

if [ "${tool}" == "osprey" -o "${tool}" == "baseline" ]; then
	mkswap /dev/sdb2
	ssh -i /home/yicheng/.ssh/id_rsa yicheng@${other_ip} "fastsudo mkswap /dev/sdb2"
	swapon /dev/sdb2
	ssh -i /home/yicheng/.ssh/id_rsa yicheng@${other_ip} "fastsudo swapon /dev/sdb2"

	ssh -i /home/yicheng/.ssh/id_rsa yicheng@${other_ip} "cd ${playground_dir}; fastsudo ${tool_cmd} ${tool_args_evaluator} ${MAGE} halfgates ${playground_dir}/config.yaml 0 0 ${workload}_${input_size}" | tee -a ${log_file}.evaluator &
	sleep 1
	${tool_cmd} ${tool_args_garbler} ${MAGE} halfgates ${playground_dir}/config.yaml 1 0 ${workload}_${input_size} | tee -a ${log_file}.garbler
elif [ "${tool}" == "mage" ]; then
	ssh -i /home/yicheng/.ssh/id_rsa yicheng@${other_ip} "cd ${playground_dir}; fastsudo ${MAGE} halfgates ${playground_dir}/config.yaml 0 0 ${workload}_${input_size}" | tee -a ${log_file}.evaluator &
	sleep 1
	${MAGE} halfgates ${playground_dir}/config.yaml 1 0 ${workload}_${input_size} | tee -a ${log_file}.garbler
fi

mv ${workload}_${input_size}_0.output ${workload}_${input_size}_0_garbler.output

echo real output is $(od -An -w -i ${workload}_${input_size}_0_garbler.output | tail -n 2)

popd

if [ "${mem_limit}" != "" ]; then
	cgdelete memory:/osprey
	ssh -i /home/yicheng/.ssh/id_rsa yicheng@${other_ip} "fastsudo cgdelete memory:/osprey"
fi

stty sane
echo ""
