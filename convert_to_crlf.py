def convert_to_crlf(input_file, output_file):
    with open(input_file, 'r', encoding='utf-8', errors='ignore') as file:
        content = file.read()

    # Replace single \r or \n with \r\n (handles mixed line endings)
    content = content.replace('\r\n', '\n').replace('\r', '\n')
    content = content.replace('\n', '\r\n')

    with open(output_file, 'w', encoding='utf-8', errors='ignore') as file:
        file.write(content)

# Replace 'input.txt' with your input file name and 'output.txt' with your desired output file name
user = "TOMAS"
convert_to_crlf(user + "/cur/sample_cur.txt", user + "/cur/sample_cur_crlf.txt")