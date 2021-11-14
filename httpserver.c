#include <err.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <pthread.h>

#include <stdio.h>
// remove later

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

pthread_mutex_t qmutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t logmutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t lock_status = PTHREAD_COND_INITIALIZER;

// I am reusing my integerqueue.c code from course
// CSE15 taught by Professsor Tantalo which uses the same queue
// data structure.
// My data structure is remade with for the purpose of this
// assignment using the influence of Jacob Sorber's
// implementation of a queue data structure in his youtube video
// on the implementation of a multi-threaded server.

// create node obj
typedef struct NodeObj {
    //since thread takes in pointer, client socket variable is a int pointer rather than a int
    int *client_socket;
    struct NodeObj* next;
}NodeObj;

typedef NodeObj* Node;

Node NewNode(int *i) {
	Node N = malloc(sizeof(NodeObj));
	N->client_socket = i;
	N->next = NULL;
	return N;
}

Node head = NULL;
Node tail = NULL;

// queues up process
// process will leave queue via dequeue
// if there is a worker looking for work.
// all nodes will eventually be dequeued,
// thus freeing is fine if done only by dequeue
void enqueue(int *client_socket){
    Node Q = NewNode(client_socket);
    if(tail == NULL){
        head = Q;
    }
    else{
        tail->next = Q;
    }
    tail = Q;
}

// dequeues process
// if head is empty, means no processes in queue
// else there exists a process waiting, returns
// the socket back into the server to execute
// while moving the pointer to the next node
int* dequeue(){
    if(head == NULL){
        return NULL;
    }
    else{
        int *currout = head->client_socket;
        Node temp = head;
        head = head->next;
        if(head == NULL){
            tail = NULL;
        }
        free(temp);
        return currout;
    }
}

// create log obj
typedef struct LogObj{
  char* logfname;
  uint16_t logport;
}LogObj;

typedef LogObj* Log;

Log newLog(char* name, uint16_t port){
  Log L = malloc(sizeof(LogObj));
  L->logfname = name;
  L->logport = port;
  return L;
}

/**
   Converts a string to an 16 bits unsigned integer.
   Returns 0 if the string is malformed or out of the range.
 */
uint16_t strtouint16(char number[]) {
  char *last;
  long num = strtol(number, &last, 10);
  if (num <= 0 || num > UINT16_MAX || *last != '\0') {
    return 0;
  }
  return num;
}

/**
   Creates a socket for listening for connections.
   Closes the program and prints an error message on error.
 */
int create_listen_socket(uint16_t port) {
  struct sockaddr_in addr;
  int listenfd = socket(AF_INET, SOCK_STREAM, 0);
  if (listenfd < 0) {
    err(EXIT_FAILURE, "socket error");
  }

  memset(&addr, 0, sizeof addr);
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htons(INADDR_ANY);
  addr.sin_port = htons(port);
  if (bind(listenfd, (struct sockaddr*)&addr, sizeof addr) < 0) {
    err(EXIT_FAILURE, "bind error");
  }

  if (listen(listenfd, 500) < 0) {
    err(EXIT_FAILURE, "listen error");
  }

  return listenfd;
}




void * handle_connection(void *p_connfd, void *p_log) {
  int connfd = *((int*)p_connfd);
  Log logfile = (Log)p_log;
  free(p_connfd);

  // check for connection and recieve request
  printf("received connection. \n\n");
  int reqbufsize = 32768;
  char* reqbuf = calloc(reqbufsize+1, sizeof(char));
  int recvsize = recv(connfd, reqbuf, reqbufsize, 0);

  //in case reqbuf is changed, have a copy
  char* tempbuf = malloc(strlen(reqbuf)+1);
  strcpy(tempbuf, reqbuf);
  tempbuf[strlen(tempbuf)] = 0;


  // print out request to check data
  write(STDOUT_FILENO, "'", 1);
  write(STDOUT_FILENO, reqbuf, recvsize);
  write(STDOUT_FILENO, "'\n", 2);


  // take requestbuffer and get request
  //
  char* end = strstr(reqbuf, "\r\n");
  char* buffer = 0;
  if (end) {
      buffer = malloc((end - reqbuf) + 1);
      if (buffer) {
          memcpy(buffer, reqbuf, end - reqbuf);
          buffer[end - reqbuf] = 0;
      }
  }

  char* host;
  int logfd;
  if(logfile != 0){
    //grab the name of the log file, and prepare the file.
    //need to free logfname
    char* logfname = malloc(strlen(logfile->logfname) + 1);
    strcpy(logfname, logfile->logfname);
    logfname[strlen(logfname)] = 0;

    logfd = open(logfname, O_WRONLY | O_APPEND);

    //convert the port into a str to find host
    int portnum = logfile->logport;
    char* str_portnum = malloc(sizeof(char)*(int)log10(portnum) + 2);
    sprintf(str_portnum, "%d", portnum);

    //get the hostbuffer
    char* s = 0;
    char* hostbuf = 0;
    char* hosttoken = strtok_r(reqbuf, " ", &s);
    while (hosttoken != NULL){
      if (strstr(hosttoken, str_portnum) != 0){
        hostbuf = malloc(strlen(hosttoken) + 1);
        strcpy(hostbuf, hosttoken);
        hostbuf[strlen(hostbuf)] = 0;
      }
      hosttoken = strtok_r(NULL, " ", &s);
    }
    
    // parse host buffer for the host
    char* split = strtok_r(hostbuf, "\n", &s);
    host = malloc(strlen(split)+1);
    strcpy(host, split);
    host[strlen(host)-1] = 0;

    //what i can currently free!
    free(str_portnum);
    free(hostbuf);
    free(logfname);
  }


  // Parse first line for type of request, file name, and version.
  // gained insight on why copying the token is better
  // reason is becasue token can change, thus I should copy the token
  // to keep its type
  char* p_req ;
  char* type = 0;
  char* filename = 0;
  char* version = 0;
  char* token = strtok_r(buffer, " ", &p_req);
  while(token != NULL){
    if (strcmp(token, "GET") == 0 || strcmp(token, "PUT") == 0 || strcmp(token, "HEAD") == 0){
      type = malloc(strlen(token) + 1);
      strcpy(type, token);
      type[strlen(type)] = 0;
    }
    else if(strstr(token, "HTTP") != 0){
      version = malloc(strlen(token) + 1);
      strcpy(version, token);
      version[strlen(version)] = 0;
    }
    else{
      filename = malloc(strlen(token) + 1);
      strcpy(filename, token);
      filename[strlen(filename)] = 0;
      memmove(&filename[0], &filename[0 + 1], strlen(filename) - 0);
    }
    token = strtok_r(NULL, " ", &p_req);
  }

  //flag to check if file name is alphanumeric
  int valid = 0;
  int counter = 0;
  char temp[1];
  while(counter < strlen(filename)){
    temp[0] = filename[counter];
    if(isalnum(temp[0]) < 1){
      valid -= 1;
    }
    counter ++;
  }
  //add to function of flag, check if version is correct
  if(strcmp(version, "HTTP/1.1") != 0){
    valid = -1;
  }


  // check if the filename fits requirements, if not send 400 Bad Request, end connection
  if(strlen(filename) < 15 || strlen(filename) > 15 || valid < 0 || version == NULL){

    if(logfile != 0){
      //write into log
      write(logfd, "FAIL\t", 5);
      write(logfd, type, strlen(type));
      write(logfd, " /", 2);
      write(logfd, filename, strlen(filename));
      write(logfd, " ", 1);
      write(logfd, version, strlen(version));
      write(logfd, "\t400\n", 5);
    }

    char* response = "HTTP/1.1 400 Bad Request\r\nContent-Length: 12\r\n\r\nBad Request\n";
    send(connfd, response, strlen(response), 0);
    free(reqbuf);
    free(tempbuf);
    free(buffer);
    free(type);
    free(filename);
    free(version);
    if(logfile != 0){
      free(host);
      close(logfd);
    }
    close(connfd);
    return NULL;
  }


  // Based on request type, send response accordingly
  if(strcmp(type, "GET") == 0){

    // get size and content of file:
    struct stat buf;
    stat(filename, &buf);
    int filesize = buf.st_size;
    int tempfile = open(filename, O_RDONLY);

    // if the file is a regular file, begin to check if it exists and permissions
    if (tempfile < 0){
      // the file does not exist
      if (errno == 2){

        if(logfile != 0){
          //write into log
          write(logfd, "FAIL\t", 5);
          write(logfd, type, strlen(type));
          write(logfd, " /", 2);
          write(logfd, filename, strlen(filename));
          write(logfd, " ", 1);
          write(logfd, version, strlen(version));
          write(logfd, "\t404\n", 5);
        }

        char* response = "HTTP/1.1 404 Not Found\r\nContent-Length: 10\r\n\r\nNot Found\n";
        send(connfd, response, strlen(response), 0);
      }
      // the file cannot be read
      else{

        if(logfile != 0){
          //write into log
          write(logfd, "FAIL\t", 5);
          write(logfd, type, strlen(type));
          write(logfd, " /", 2);
          write(logfd, filename, strlen(filename));
          write(logfd, " ", 1);
          write(logfd, version, strlen(version));
          write(logfd, "\t403\n", 5);
        }

        char* response = "HTTP/1.1 403 Forbidden\r\nContent-Length: 10\r\n\r\nForbidden\n";
        send(connfd, response, strlen(response), 0);
      }
    }
    else{
      // check if the exsisting file is a regular file
      // if not, send bad req
      if(S_ISREG(buf.st_mode) < 1){

        if(logfile != 0){
          //write into log
          write(logfd, "FAIL\t", 5);
          write(logfd, type, strlen(type));
          write(logfd, " /", 2);
          write(logfd, filename, strlen(filename));
          write(logfd, " ", 1);
          write(logfd, version, strlen(version));
          write(logfd, "\t400\n", 5);
        }

        char* response = "HTTP/1.1 400 Bad Request\r\nContent-Length: 12\r\n\r\nBad Request\n";
        send(connfd, response, strlen(response), 0);
        free(reqbuf);
        free(tempbuf);
        free(buffer);
        free(type);
        free(version);
        free(filename);
        if(logfile != 0){
          free(host);
          close(logfd);
        }
        close(connfd);
        return NULL;
      }
      else{
        //store content into buffer
        char* contentbuf = calloc(filesize, sizeof(char));
        int size = 0;
        while (read(tempfile, contentbuf, filesize) > 0){
          size ++;
        }

        // init size to str
        char* contentlength = malloc(sizeof(char)*(int)log10(filesize) + 2);
        sprintf(contentlength, "%d", filesize);

        if(logfile != 0){
          //write into log
          write(logfd, type, strlen(type)); 
          write(logfd, "\t/", 2);
          write(logfd, filename, strlen(filename));
          write(logfd, "\t", 1);
          write(logfd, host, strlen(host));
          write(logfd, "\t", 1);
          write(logfd, contentlength, strlen(contentlength));
          write(logfd, "\n\0", 1);
        }

        char status[] = "HTTP/1.1 200 OK\r\nContent-Length: ";
        char endline[] = "\r\n\r\n";
        int responselen = strlen(status) + strlen(contentlength) + strlen(endline);
        char* response = calloc(responselen + 1, sizeof(char));
        strcat(response, status);
        strcat(response, contentlength);
        strcat(response, endline);

        send(connfd, response, responselen, 0);
        send(connfd, contentbuf, filesize, 0);
        free(response);
        free(contentbuf);
        free(contentlength);
      }
    }
  }

  else if(strcmp(type, "HEAD") == 0){

    // get size and content of file:
    struct stat buf;
    stat(filename, &buf);
    int filesize = buf.st_size;
    int tempfile = open(filename, O_RDONLY);


    // if the file is a regular file, begin to check if it exists and permissions
    if (tempfile < 0){
     if (errno == 2){

       if(logfile != 0){
      //write into log
        write(logfd, "FAIL\t", 5);
        write(logfd, type, strlen(type));
        write(logfd, " /", 2);
        write(logfd, filename, strlen(filename));
        write(logfd, " ", 1);
        write(logfd, version, strlen(version));
        write(logfd, "\t404\n", 5);
      }

        char* response = "HTTP/1.1 404 Not Found\r\nContent-Length: 10\r\n\r\nNot Found\n";
        send(connfd, response, strlen(response), 0);
      }
      // the file cannot be read
      else{

        if(logfile != 0){
          //write into log
          write(logfd, "FAIL\t", 5);
          write(logfd, type, strlen(type));
          write(logfd, " /", 2);
          write(logfd, filename, strlen(filename));
          write(logfd, " ", 1);
          write(logfd, version, strlen(version));
          write(logfd, "\t403\n", 5);
        }

        char* response = "HTTP/1.1 403 Forbidden\r\nContent-Length: 10\r\n\r\nForbidden\n";
        send(connfd, response, strlen(response), 0);
      }
    }
    else{
      // check if the exsisting file is a regular file
      // if not, send bad req
      if(S_ISREG(buf.st_mode) < 1){

        if(logfile != 0){
          //write into log
          write(logfd, "FAIL\t", 5);
          write(logfd, type, strlen(type));
          write(logfd, " /", 2);
          write(logfd, filename, strlen(filename));
          write(logfd, " ", 1);
          write(logfd, version, strlen(version));
          write(logfd, "\t400\n", 5);
        }

        char* response = "HTTP/1.1 400 Bad Request\r\nContent-Length: 12\r\n\r\nBad Request\n";
        send(connfd, response, strlen(response), 0);
        free(reqbuf);
        free(tempbuf);
        free(buffer);
        free(type);
        free(version);
        free(filename);
        if(logfile != 0){
          free(host);
          close(logfd);
        }
        close(connfd);
        return NULL;
      }
      else{
        char* contentlength = malloc(sizeof(char)*(int)log10(filesize) + 2);
        sprintf(contentlength, "%d", filesize);

        if(logfile != 0){
          //write into log
          write(logfd, type, strlen(type)); 
          write(logfd, "\t/", 2);
          write(logfd, filename, strlen(filename));
          write(logfd, "\t", 1);
          write(logfd, host, strlen(host));
          write(logfd, "\t", 1);
          write(logfd, contentlength, strlen(contentlength));
          write(logfd, "\n\0", 1);
        }

        char status[] = "HTTP/1.1 200 OK\r\nContent-Length: ";
        char endline[] = "\r\n\r\n";
        int responselen = strlen(version) + strlen(status)+ 1;
        char* response = calloc(responselen, sizeof(char));
        strcat(response, status);
        strcat(response, contentlength);
        strcat(response, endline);

        send(connfd, response, strlen(response), 0);
        free(contentlength);
        free(response);
      }
    }
  }



  else if(strcmp(type, "PUT") == 0){

    // parse for content length header
    char *shared_p;
    char* contentheader;
    char* contenttoken = strtok_r(tempbuf, "\r\n", &shared_p);
    while(contenttoken != NULL){
      if(strstr(contenttoken, "Content-Length") != 0){
        contentheader = malloc(strlen(contenttoken) + 1);
        strcpy(contentheader, contenttoken);
        contentheader[strlen(contentheader)] = 0;
      }
      contenttoken = strtok_r(NULL, "\r\n", &shared_p);
    }

    //grab the content length from content length header
    int contentlength;
    char words[14];
    sscanf (contentheader, " %s %d", words, &contentlength);

    char* str_contentlength = malloc(sizeof(char)*(int)log10(contentlength) + 2);
    sprintf(str_contentlength, "%d", contentlength);

    // put content into buffer using the content length
    char* bodybuf = calloc(contentlength, sizeof(char));
    recv(connfd, bodybuf, contentlength, 0);
    int tempfile = open(filename, O_WRONLY);

    // check if the file exists
    // if not, create a file with the given filename
    if(tempfile < 0){
      tempfile = open(filename, O_WRONLY | O_CREAT);
      write(tempfile, bodybuf, contentlength);
      close(tempfile);

      if(logfile != 0){
        //write into log
        write(logfd, type, strlen(type)); 
        write(logfd, "\t/", 2);
        write(logfd, filename, strlen(filename));
        write(logfd, "\t", 1);
        write(logfd, host, strlen(host));
        write(logfd, "\t", 1);
        write(logfd, str_contentlength, strlen(str_contentlength));
        write(logfd, "\n\0", 1);
      }

      char* response = "HTTP/1.1 201 Created\r\nContent-Length: 8\r\n\r\nCreated\n";
      send(connfd, response, strlen(response), 0);
      free(bodybuf);
      free(str_contentlength);
      free(contentheader);
    }

    else{
      if(access(filename, W_OK)<0){

        if(logfile != 0){
          //write into log
          write(logfd, "FAIL\t", 5);
          write(logfd, type, strlen(type));
          write(logfd, " /", 2);
          write(logfd, filename, strlen(filename));
          write(logfd, " ", 1);
          write(logfd, version, strlen(version));
          write(logfd, "\t403\n", 5);
        }

        char* response = "HTTP/1.1 403 Forbidden\r\nContent-Length: 10\r\n\r\nForbidden\n";
        send(connfd, response, strlen(response), 0);
        free(bodybuf);
      }
      else{
        write(tempfile, bodybuf, contentlength);
        close(tempfile);

        if(logfile != 0){
          //write into log
          write(logfd, type, strlen(type)); 
          write(logfd, "\t/", 2);
          write(logfd, filename, strlen(filename));
          write(logfd, "\t", 1);
          write(logfd, host, strlen(host));
          write(logfd, "\t", 1);
          write(logfd, str_contentlength, strlen(str_contentlength));
          write(logfd, "\n\0", 1);
        }

        char* response = "HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\nOK\n";
        send(connfd, response, strlen(response), 0);
        free(bodybuf);
        free(str_contentlength);
        free(contentheader);
      }
    }
  }

  else{
    // head, get, or put is not the request

    if(logfile != 0){
      //write into log
      write(logfd, "FAIL\t", 5);
      write(logfd, type, strlen(type));
      write(logfd, " /", 2);
      write(logfd, filename, strlen(filename));
      write(logfd, " ", 1);
      write(logfd, version, strlen(version));
      write(logfd, "\t501\n", 5);
    }

    char* response = "HTTP/1.1 501 Not Implemented\r\nContent-Length: 16\r\n\r\nNot Implemented\n";
    send(connfd, response, strlen(response), 0);
  }   
    


  printf("\nthis is the end of function!\n");
  // when done, close socket
  free(reqbuf);
  free(buffer);
  free(tempbuf);
  free(type);
  free(version);
  free(filename);
  //free memory allocated if there exists a log feature!
  if(logfile != 0){
    free(host);
    close(logfd);
  }
  close(connfd);
  return NULL;
}



//worker function to process threads!
void * worker(void *arg){
  while(1){
    int *p_connfd;
    Log input_log = (Log)arg;
    pthread_mutex_lock(&qmutex);
    if((p_connfd = dequeue()) == NULL){
      pthread_cond_wait(&lock_status, &qmutex);
      p_connfd = dequeue();
    }
    pthread_mutex_unlock(&qmutex);
    if (p_connfd != NULL){
      pthread_mutex_lock(&logmutex);
      handle_connection(p_connfd, input_log);
      pthread_mutex_unlock(&logmutex);
    }
  }
}




int main(int argc, char *argv[]) {
  int listenfd;
  char* shared_p;
  uint16_t port;
  char* log_arg = 0;
  int num_threads = 4;

  int opt;

  //if argument is less than 2, obviously not going to work.
  if (argc < 2) {
    errx(EXIT_FAILURE, "wrong arguments: %s port_num", argv[0]);
  }

  //parse arguements if argv >= 2
  while((opt = getopt(argc, argv, "l:N:")) != -1){
    switch(opt){
      case 'l':
        log_arg = optarg;
        break;
      case 'N':
        num_threads = strtol(optarg, &shared_p, 10);
        break;
    }
  }
  //make left over argument into int
  for(; optind < argc; optind++){
    port = strtouint16(argv[optind]);
  }

  //if there is a log_arg, aka '-l'
  Log input_log = 0;
  if(log_arg != 0){
    input_log = newLog(log_arg, port);
    int logfd = open(log_arg, O_CREAT|O_WRONLY|O_TRUNC, S_IRWXU|S_IRWXG|S_IRWXO);
    close(logfd);
  }

  //if port is not valid, terminate
  if (port == 0) {
    errx(EXIT_FAILURE, "invalid port number: %s", argv[1]);
  }
  listenfd = create_listen_socket(port);

  // create thread pool
  pthread_t thread_pool[num_threads];

  for(int i = 0; i < num_threads; i ++){
    pthread_create(&thread_pool[i], NULL, worker, input_log);
  }

  // dispatcher
  // assign/queue up process for the threads
  while(1) {
    int connfd = accept(listenfd, NULL, NULL);
    if (connfd < 0) {
      warn("accept error");
      continue;
    }

    int *p_connfd = malloc(sizeof(int));
    *p_connfd = connfd;
    pthread_mutex_lock(&qmutex);
    enqueue(p_connfd);
    pthread_cond_signal(&lock_status);
    pthread_mutex_unlock(&qmutex);
    
  }
  return EXIT_SUCCESS;
}
