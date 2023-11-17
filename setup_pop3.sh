#!/bin/bash

port=110  # Default port
path="./Maildir"  # Default path

while getopts ":p:u:d:" opt; do
    case $opt in
        p) port="$OPTARG"
        ;;
        u) users="$OPTARG"
        ;;
        d) path="$OPTARG"
        ;;
        \?) echo "Invalid option -$OPTARG" >&2
        ;;
    esac
done

# Check if users are provided
if [ -z "$users" ]; then
    echo "No users provided. Exiting."
    exit 1
fi

IFS=' ' read -r -a user_array <<< "$users"

for user_pass in "${user_array[@]}"; do
    IFS=':' read -r -a user_pass_array <<< "$user_pass"
    user="${user_pass_array[0]}"
    user_dir="$path/$user"

    mkdir -p "$user_dir/cur" "$user_dir/new" "$user_dir/tmp"

    # Add sample mails to 'new' and 'cur' folders
    echo "Subject: Welcome to POP3 Server" > "$user_dir/new/welcome_new.txt"
    echo "Subject: Sample Current Email" > "$user_dir/cur/sample_cur.txt"
done

echo "POP3 server folders set up at $path for port $port"