The server is executed with ./httpserver port_num
Two features that can be used along with the execution includes -N num_threads and -l logfile_name
These features can apply in any order (suchas  port_num|logfile_name, port_num|num_threads, port_num, logfile_name| etc.)

Resources used to accomplish this assignment includes Jacob Sorber's implementation of a multithreaded server, Piazza, manual for system calls and certain tutorialspoint. I also aquired advice from Sunny Zhang for certain implementation errors that I was not unable to understand such as why I was cosntantly bombarded with -1 file discripter calls and identifying the differences between strtok and strtok_r.

Links:

getopt example
https://www.tutorialspoint.com/getopt-function-in-c-to-parse-command-line-arguments

Patrick Tantalo's CSE 15 Assignment: Integer Queue

Multithreaded server by Jacob Sorber
https://www.youtube.com/watch?v=Pg_4Jz8ZIH4&t=758s