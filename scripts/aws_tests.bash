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

for INSTANCE_NAME in "${INSTANCES_x86_64[@]}" "${INSTANCES_aarch64[@]}"; do
  if printf '%s\n' "${INSTANCES_aarch64[@]}" | grep -qx "${INSTANCE_NAME}"; then
    AMI_ID=${AMI_ID_aarch64}
  else
    AMI_ID=${AMI_ID_x86_64}
  fi
  echo "Running instance for ${INSTANCE_NAME} with AMI ${AMI_ID}"

  INSTANCE_ID=$(aws ec2 run-instances \
    --image-id ${AMI_ID} \
    --instance-type ${INSTANCE_NAME} \
    --key-name ${KEY_NAME} \
    --block-device-mappings "DeviceName=/dev/sda1,Ebs={VolumeSize=${VOLUME_SIZE}}" \
    --associate-public-ip-address \
    --security-group-ids ${SECURITY_GROUP} \
    --count "1" --query 'Instances[0].InstanceId' --output text)

  aws ec2 wait instance-status-ok --instance-ids ${INSTANCE_ID}
  echo "Started instance: ${INSTANCE_ID}"

  PUBLIC_IP=$(aws ec2 describe-instances \
    --instance-ids ${INSTANCE_ID} \
    --query "Reservations[0].Instances[0].PublicIpAddress" \
    --output text)
  echo "Instance public IP: ${PUBLIC_IP}"

  rsync -avz --partial --progress --exclude ".git" --exclude "build" -e "${SSH_COMMAND}" \
    ${PROJECT_DIR}/ ubuntu@${PUBLIC_IP}:~/${PROJECT_DIR}
  ${SSH_COMMAND} ubuntu@${PUBLIC_IP} << 'EOF'
    set -e # Exit on error

    # Install dependencies
    sudo apt update
    sudo apt install -y linux-tools-common linux-tools-generic g++ cmake

    # Build the project
    cmake -B build . && cmake --build build

    # Run the script to generate multiple tables
    ./scripts/generate_multiple_tables.py
EOF

  echo "Script executed successfully"
  mkdir -p "${PROJECT_DIR}/outputs/${INSTANCE_NAME}"
  rsync -avz --partial --progress -e "${SSH_COMMAND}" \
    ubuntu@${PUBLIC_IP}:~/${PROJECT_DIR}/outputs/ ${PROJECT_DIR}/outputs/${INSTANCE_NAME}/

  aws ec2 terminate-instances --instance-ids ${INSTANCE_ID}
  echo "Terminated instance: ${INSTANCE_ID}"
done
