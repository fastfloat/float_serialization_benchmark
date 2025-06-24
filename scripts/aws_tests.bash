#!/bin/bash

# Preliminary setup
# - Install AWS CLI v2 (perhaps it works on v1, untested):
#   - on Arch Linux: paru -S aws-cli-v2
# - Configure AWS Credentials: aws configure

set -e # Exit script on first error

AMI_ID_x86_64="ami-020cba7c55df1f615"
AMI_ID_aarch64="ami-07041441b708acbd6"

INSTANCES_x86_64=()
INSTANCES_aarch64=("c6g.medium" "c7g.medium" "c8g.medium")

KEY_NAME="openssh"
KEY_PATH="~/.ssh/openssh.pem"
SSH_COMMAND="ssh -i ${KEY_PATH} -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null"
SECURITY_GROUP="sg-0315466a0d7fc99e1"
VOLUME_SIZE=10 # in GB

PROJECT_DIR=$(basename $(pwd))

process_instance() {
  INSTANCE_NAME=$1
  AMI_ID=$2
  echo "Running instance for ${INSTANCE_NAME} with AMI ${AMI_ID}"

  INSTANCE_ID=$(aws ec2 run-instances \
    --image-id ${AMI_ID} \
    --instance-type ${INSTANCE_NAME} \
    --key-name ${KEY_NAME} \
    --block-device-mappings "DeviceName=/dev/sda1,Ebs={VolumeSize=${VOLUME_SIZE}}" \
    --associate-public-ip-address \
    --security-group-ids ${SECURITY_GROUP} \
    --count "1" --query 'Instances[0].InstanceId' --output text)

  echo "Waiting for instance ${INSTANCE_ID} to be ready..."
  aws ec2 wait instance-status-ok --instance-ids ${INSTANCE_ID}
  echo "Started instance: ${INSTANCE_ID}"

  PUBLIC_IP=$(aws ec2 describe-instances \
    --instance-ids ${INSTANCE_ID} \
    --query "Reservations[0].Instances[0].PublicIpAddress" \
    --output text)
  echo "Instance ${INSTANCE_ID} public IP: ${PUBLIC_IP}"

  git ls-files -z | rsync -avz --partial --progress --from0 --files-from=- -e "${SSH_COMMAND}" \
    ./ ubuntu@${PUBLIC_IP}:~/${PROJECT_DIR}
  ${SSH_COMMAND} ubuntu@${PUBLIC_IP} << EOF
    set -e # Exit on error
    cd ~/${PROJECT_DIR}

    echo "Updating and installing dependencies on ${INSTANCE_NAME}..."
    sudo apt update
    sudo DEBIAN_FRONTEND=noninteractive \
         apt install -y linux-tools-common linux-tools-generic \
                        g++ clang cmake python3
    echo -1 | sudo tee /proc/sys/kernel/perf_event_paranoid

    echo "Saving some info about the environment..."
    mkdir -p outputs
    lscpu > outputs/lscpu.txt
    g++ --version > outputs/g++.txt
    clang++ --version > outputs/clang++.txt

    echo "Building project with g++ and running the benchmarks..."
    CXX=g++ cmake -B build . && cmake --build build
    ./scripts/generate_multiple_tables.py g++

    rm -rf build

    echo "Building project with clang++ and running the benchmarks..."
    CXX=clang++ cmake -B build . && cmake --build build
    ./scripts/generate_multiple_tables.py clang++
EOF

  echo "Script executed successfully on ${INSTANCE_NAME}"
  mkdir -p "./outputs/${INSTANCE_NAME}"
  rsync -avz --partial --progress -e "${SSH_COMMAND}" \
    ubuntu@${PUBLIC_IP}:~/${PROJECT_DIR}/outputs/ ./outputs/${INSTANCE_NAME}/

  aws ec2 terminate-instances --instance-ids ${INSTANCE_ID}
  echo "Terminated instance: ${INSTANCE_ID}"
}

echo "Launching ${#INSTANCES_aarch64[@]} aarch64 instances and ${#INSTANCES_x86_64[@]} x86_64 instances in parallel..."
for INSTANCE_NAME in "${INSTANCES_x86_64[@]}" "${INSTANCES_aarch64[@]}"; do
  if printf '%s\n' "${INSTANCES_aarch64[@]}" | grep -qx "${INSTANCE_NAME}"; then
    AMI_ID=${AMI_ID_aarch64}
  else
    AMI_ID=${AMI_ID_x86_64}
  fi

  process_instance "${INSTANCE_NAME}" "${AMI_ID}" 2>&1 | tee "${INSTANCE_NAME}.log" &
done

# Wait for all background jobs to finish
wait
echo "All instances completed."
