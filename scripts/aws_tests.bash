#!/bin/bash

# This script launches EC2 instances to benchmark your project.
#
# Requirements:
# - The programs `git`, `ssh`, `rsync`, and `aws` must be installed.
# - AWS CLI v2 installed and configured (`aws configure`)
# - An EC2-compatible SSH key must exist in AWS, or the script will generate one (and save locally).
#
# Required AWS IAM permissions:
# - ec2:RunInstances
# - ec2:TerminateInstances
# - ec2:DescribeInstances
# - ec2:DescribeVpcs
# - ec2:CreateSecurityGroup
# - ec2:DeleteSecurityGroup
# - ec2:AuthorizeSecurityGroupIngress
#
# Optional environment variables:
#   AWS_KEY_NAME         use an existing key pair instead of creating one
#   AWS_SECURITY_GROUP   use an existing SG instead of creating one

set -euo pipefail

# --------------------
# User-configurable variables
# --------------------

# Ubuntu 24.04 AMI IDs for x86_64 and aarch64 architectures
declare -A AMI_MAP=(
  ["x86_64"]="ami-020cba7c55df1f615"
  ["aarch64"]="ami-07041441b708acbd6"
)

# We need biggest (metal) instances to access perf events on x86
INSTANCES_x86_64=(
  "c5n.metal"        # Skylake
  "c6i.metal"        # Ice Lake
  "c7i.metal-24xl"   # Sapphire Rapids
  "c5a.24xlarge"     # EPYC Zen 2
  "c6a.metal"        # EPYC Zen 3
  "c7a.metal-48xl"   # EPYC Zen 4
)
INSTANCES_aarch64=(
  "c6g.medium"  # Graviton 2 - Neoverse N1
  "c7g.medium"  # Graviton 3 - Neoverse V1
  "c8g.medium"  # Graviton 4 - Neoverse V2
)

VOLUME_SIZE=10 # in GB

KEY_NAME="${AWS_KEY_NAME:-aws_auto}" # Key path is assumed to be ~/.ssh/${KEY_NAME}.pem
SECURITY_GROUP="${AWS_SECURITY_GROUP:-}"

RETRIES=5 # Number of retries when a problem occurs during instance processing

# --------------------
# Internal variables (do not modify)
# --------------------

KEY_PATH="$HOME/.ssh/${KEY_NAME}.pem"
SSH_COMMAND="ssh -i ${KEY_PATH} -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null"

PROJECT_DIR=$(basename "$(git rev-parse --show-toplevel)")
CREATED_SECURITY_GROUP=""

# Cleanup function to delete created security group on exit
cleanup() {
  if [ -n "${CREATED_SECURITY_GROUP}" ]; then
    if aws ec2 delete-security-group --group-id "${CREATED_SECURITY_GROUP}"; then
      echo "Cleaned up security group: ${CREATED_SECURITY_GROUP}"
    else
      echo "Failed to clean up security group ${CREATED_SECURITY_GROUP}; you should delete it manually."
    fi
  fi
}

check_prerequisites() {
  if (( BASH_VERSINFO[0] < 4 )); then
    echo "Error: This script requires Bash version 4 or higher." >&2
    exit 1
  fi

  for cmd in git ssh rsync aws; do
    if ! command -v "$cmd" >/dev/null 2>&1; then
      echo "Error: Required command '$cmd' is not installed." >&2
      exit 1
    fi
  done

  if ! git rev-parse --show-toplevel >/dev/null 2>&1; then
    echo "Error: This script must be run from within a Git repository." >&2
    exit 1
  fi

  if ! aws sts get-caller-identity >/dev/null 2>&1; then
    echo "Error: AWS credentials not configured. Run 'aws configure'." >&2
    exit 1
  fi
}

create_key_pair() {
  if aws ec2 describe-key-pairs --key-names "${KEY_NAME}" >/dev/null 2>&1; then
    echo "Using existing AWS key pair: ${KEY_NAME}"
    return
  fi

  echo "Creating new AWS key pair named ${KEY_NAME}"
  mkdir -p ~/.ssh
  [ -f "$KEY_PATH" ] && rm -f "$KEY_PATH"
  aws ec2 create-key-pair --key-name "$KEY_NAME" \
    --query 'KeyMaterial' --output text > "$KEY_PATH"
  chmod 400 "$KEY_PATH"
  echo "Created and saved key pair private key to $KEY_PATH"
}

create_security_group() {
  if [ -n "${SECURITY_GROUP}" ]; then
    echo "Using existing security group: ${SECURITY_GROUP}"
    return
  fi

  echo "Creating a new security group for SSH access..."
  VPC_ID=$(aws ec2 describe-vpcs \
          --filters Name=isDefault,Values=true \
          --query "Vpcs[0].VpcId" \
          --output text)

  CREATED_SECURITY_GROUP=$(aws ec2 create-security-group \
          --group-name ssh-public-access \
          --description "Allow SSH access from anywhere (0.0.0.0/0)" \
          --vpc-id "${VPC_ID}" \
          --query "GroupId" \
          --output text)

  aws ec2 authorize-security-group-ingress \
    --group-id "${CREATED_SECURITY_GROUP}" \
    --protocol tcp \
    --port 22 \
    --cidr 0.0.0.0/0 \
    --no-cli-pager

  SECURITY_GROUP="${CREATED_SECURITY_GROUP}"
  echo "Created security group: ${SECURITY_GROUP}"
}

get_arch() {
  local instance_name="$1"
  if printf '%s\n' "${INSTANCES_aarch64[@]}" | grep -qx "$instance_name"; then
    echo "aarch64"
  else
    echo "x86_64"
  fi
}

ensure_ssh_ready() {
  local ip="$1"
  for i in {1..30}; do
    if ${SSH_COMMAND} ubuntu@${ip} "echo SSH Ready" >/dev/null 2>&1; then
      return 0
    fi
    sleep 5
  done
  echo "SSH did not become ready on ${ip} after waiting."
  return 1
}

do_remote_work() {
  local ip="$1"
  git ls-files -z | rsync -avz --partial --progress --from0 --files-from=- -e "${SSH_COMMAND}" \
    ./ ubuntu@${ip}:~/${PROJECT_DIR} || return 1

  ${SSH_COMMAND} ubuntu@${ip} << EOF
    set -e # Exit on error
    cd ~/${PROJECT_DIR}

    echo "Updating and installing dependencies..."
    sudo apt update
    sudo DEBIAN_FRONTEND=noninteractive apt install -y \
      linux-tools-common linux-tools-generic g++ clang cmake python3

    # Enable access to perf events for benchmarking
    # Must use 'sudo tee' since shell redirection ('>') is not affected by sudo
    echo -1 | sudo tee /proc/sys/kernel/perf_event_paranoid > /dev/null

    echo "Saving some info about the environment..."
    mkdir -p outputs
    lscpu > outputs/lscpu.txt
    g++ --version > outputs/g++.txt
    clang++ --version > outputs/clang++.txt

    echo "Building project with g++ and running the benchmarks..."
    CC=gcc CXX=g++ cmake -B build . && cmake --build build
    ./scripts/generate_multiple_tables.py g++

    rm -rf build

    echo "Building project with clang++ and running the benchmarks..."
    CC=clang CXX=clang++ cmake -B build . && cmake --build build
    ./scripts/generate_multiple_tables.py clang++
EOF
}

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
    --query "Reservations[0].Instances[0].PublicIpAddress" --output text)
  echo "Instance ${INSTANCE_ID} public IP: ${PUBLIC_IP}"
  ensure_ssh_ready "${PUBLIC_IP}"

  for attempt in $(seq 1 ${RETRIES}); do
    if do_remote_work "${PUBLIC_IP}"; then
      echo "Remote work completed successfully"
      break
    else
      echo "Attempt ${attempt} failed, retrying..."
      sleep 10
    fi
  done

  mkdir -p "./outputs/${INSTANCE_NAME}"
  rsync -avz --partial --progress -e "${SSH_COMMAND}" \
    ubuntu@${PUBLIC_IP}:~/${PROJECT_DIR}/outputs/ ./outputs/${INSTANCE_NAME}/ \
    || echo "Failed to copy outputs from ${PUBLIC_IP}"

  if aws ec2 terminate-instances --instance-ids ${INSTANCE_ID}; then
    echo "Terminated instance: ${INSTANCE_ID}"
  else
    echo "Failed to terminate instance ${INSTANCE_ID}; you should terminate it manually."
  fi
}

main () {
  trap cleanup EXIT
  check_prerequisites
  create_key_pair
  create_security_group

  echo "Launching ${#INSTANCES_aarch64[@]} aarch64 instances and ${#INSTANCES_x86_64[@]} x86_64 instances in parallel..."
  for INSTANCE_NAME in "${INSTANCES_aarch64[@]}" "${INSTANCES_x86_64[@]}"; do
    ARCH=$(get_arch "$INSTANCE_NAME")
    AMI_ID="${AMI_MAP[$ARCH]}"

    process_instance "${INSTANCE_NAME}" "${AMI_ID}" 2>&1 | tee "${INSTANCE_NAME}.log" &
    sleep 1
  done

  # Wait for all background jobs to finish
  wait
  echo "All instances completed."
}

if [ "$0" = "$BASH_SOURCE" ] ; then
  main
fi
