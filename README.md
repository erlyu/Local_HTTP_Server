## Multithreaded Web Server

### Introduction
The Purpose of this project is built to simulate HTTP servers with the use of mutithreading as well as logging capabilities. It takes in three seperate types of requests: GET, HEAD, and PUT, and preformes some action based on the request.

### Technologies
* Programming Language C
* Dynamic Queue Data Structure
* Pthread library for Multithreading
* Mutex Locks for Mutal Exclusion
* Socket Programming
* Getopt Argument Parsing

### Prerequisite
* Linux [Ubuntu 20.04](https://ubuntu.com/download/desktop) environment
* Clang compiler for C
```bash
sudo apt install build-essential clang git make valgrind lldb clang-format
```

### Instruction/Use
##### Open a terminal and select the directory where the project exists.
##### Execute the following:
```bash
make
./httpserver 3000 -l log_file
```
#### This execution will produce a text file called log_file to log requests from the client on the port number 3000 using default 4 threads.
#### The numbers of threads can be changed by including -N as shown:
```bash
./httpserver 3000 -N 6 -l log_file.txt
```

##### To send a request to the server, execute curl requests into another terminal on the same port

##### An example of a GET request is as follows:
###### An GET curl request will retrieve resources from the testfile01234567 and store into the output
```bash
curl http://localhost:3000/testfile01234567 -v -o output
```
##### An example of a HEAD request is as follows:
###### An HEAD curl request will retrieve and return the file size of testfile01234567
```bash
curl input http://localhost:3000/testfile01234567 -v
```
##### An example of a PUT request is as follows:
###### An PUT curl request will repalce the resources within the input file with testfile01234567
```bash
curl -T input http://localhost:3000/testfile01234567 -v
```