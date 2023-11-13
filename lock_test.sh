#!/bin/bash

# Path to the lock file
LOCK_FILE="./user/new/welcome_new"

# Create the lock file if it doesn't exist
touch "$LOCK_FILE"

# Open the file descriptor
exec 200>"$LOCK_FILE"

# Acquire the lock
if flock -n 200; then
    echo "Successfully acquired lock on $LOCK_FILE."
    echo "Press [CTRL+C] to stop..."
    while true
    do
        sleep 1
    done
else
    echo "Failed to acquire lock. The file is already locked."
    exit 1
fi