#!/bin/bash
# Post-stop script for L2Trader Recorder
# This script is executed after the recorder service stops (normally or by crash)

# Log the execution
logger -t l2trader-recorder-post-stop "Recorder service stopped at $(date)"

# Placeholder: Execute custom actions here
echo "hello world"

# Exit successfully
exit 0
