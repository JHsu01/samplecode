
const char * usage =
"                                                               \n"
"usage: myhttpd [-f|-t|-p] [<port>]                             \n"
"                                                               \n"
"   -f: Create a new process for each request                   \n"
"   -t: Create a new thread for each request                    \n"
"   -p: Pool of threads                                         \n"
"                                                               \n"
"   1024 < [<port>] < 65536                                     \n"
"                                                               \n"
"   Note: if no port is chosen, port 50942 is chosen            \n"
"                                                               \n";
const char *err_msg = "HTTP/1.1 401 Unauthorized\015\012WWW-Authenticate: Basic realm=\"myhttpd-cs252\"\015\012Server: data.cs.purdue.edu\015\012Content-length: 14\015\012Content-Type: text/html\015\012\015\012Not Authorized\015\012";

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <vector>
#include <string>
#include <iostream>
#include <errno.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <pthread.h>
#include <signal.h>
#include <sys/types.h>
#include <dirent.h>
#include <string>
#include <sstream>
#include <fstream>

using namespace std;

int QueueLength = 5;

pthread_mutex_t mutex_m;

string convert_file_toString(string file);
string server_resp(string file);
// Processes time request
void process_my_request(int socket);
void fork_server(int masterSocket);
void request_thread(int masterSocket);
void while_loop_basically(int masterSocket);
void pool_of_threads(int masterSocket);
void loopthread(int masterSocket);
int s_type = 0;

extern "C" void zombieTime(int sig) {
  while (waitpid(-1, NULL, WNOHANG) > 0)
    ;
}

int main( int argc, char ** argv ) {
int port = 0;
  if ( argc < 2 ) {
    //defualt port
    //note: doesn't seem to work lmao
  //  port = 50942;
   fprintf(stderr, "%s", usage);
    exit(-1);
  }
  // Get the port from the arguments
  port = atoi(argv[1]);

  // check for flags
  if (argc > 2) {
    if (strcmp(argv[1], "-f") == 0) {
      s_type = 1;
    } else if (strcmp(argv[1], "-t") == 0) {
      s_type = 2;
    } else if (strcmp(argv[1], "-p") == 0) {
      s_type = 3;
    }
      port = atoi(argv[2]);
    }
  

  // if the port number doesn't work out, print usage.
  if(port < 1024 || port > 65536) {
    fprintf(stderr, "%s", usage);
    exit(-1);
  }
      
  //check for zombies
  struct sigaction sigA;
  sigA.sa_handler = zombieTime;
  sigemptyset(&sigA.sa_mask);
  sigA.sa_flags = SA_RESTART;

  int sAerr = sigaction(SIGCHLD, &sigA, NULL);
  if(sAerr) {
    perror("sigaction");
    exit(-1);
  }

  // Set the IP address & port
  struct sockaddr_in serverIPAddress; 
  memset( &serverIPAddress, 0, sizeof(serverIPAddress));
  serverIPAddress.sin_family = AF_INET;
  serverIPAddress.sin_addr.s_addr = INADDR_ANY;
  serverIPAddress.sin_port = htons((u_short) port);
  
  // Allocate a socket
  int masterSocket =  socket(PF_INET, SOCK_STREAM, 0);
  if ( masterSocket < 0) {
    perror("socket");
    exit( -1 );
  }

  // Set socket options to reuse port. Otherwise we will
  // have to wait about 2 minutes before reusing the sae port number
  int optval = 1; 
  int err = setsockopt(masterSocket, SOL_SOCKET, SO_REUSEADDR, 
		       (char *) &optval, sizeof( int ) );
   
  // Bind the socket to the IP address and port
  int error = bind( masterSocket,
		    (struct sockaddr *)&serverIPAddress,
		    sizeof(serverIPAddress) );
  if ( error ) {
    perror("bind");
    exit( -1 );
  }
  
  // Put socket in listening mode and set the 
  // size of the queue of unprocessed connections
  error = listen( masterSocket, QueueLength);
  if ( error ) {
    perror("listen");
    exit( -1 );
  }

  if (s_type == 1) {
    // fork this
    fork_server(masterSocket);
  }
  else if (s_type == 2)
  {
    // create thread for each seperate request
    request_thread(masterSocket);
  }
  else if (s_type == 3)
  {
    // pool of threads
    pool_of_threads(masterSocket);
  }
  else
  {
    // basically a while loop that acts as the iterative server lmao
    while_loop_basically(masterSocket);
  }
}
  
   
string convert_file_toString(string file) {
  // as the description notes, this helper function takes a file and converts it to a string
  ifstream file_in(file);
  if (!file_in.is_open()) {
    exit(1);
  }
  return string((istreambuf_iterator<char>(file_in)), istreambuf_iterator<char>());
}

string server_resp(string file) {
  if (file.find("favicon.ico") != std::string::npos) {
    return "HTTP/1.1 200 OK\015\012WWW-Authenticate: Basic realm=\"myhttpd-cs252\"\015\012Server: data.cs.purdue.edu\015\012Content-length: 0\015\012Content-Type: text/html\015\012\015\012\015\012";
  }
  string fileType = "text/html";
  if (file.find(".svg") != std::string::npos) {
    fileType = "image/svg+xml";
  } else if (file.find(".gif") != std::string::npos) {
    fileType = "image/gif";
  }
  string str = convert_file_toString(file);
  int len = str.size();
  printf("FILE AS HTML = %s\n", str.c_str());
  return "HTTP/1.1 200 OK\015\012WWW-Authenticate: Basic realm=\"myhttpd-cs252\"\015\012Server: data.cs.purdue.edu\015\012Content-length: " + to_string(len) + "\015\012Content-Type: " + fileType + "\015\012\015\012" + str + "\015\012";
}


void while_loop_basically(int masterSocket) {
  // I mean, it's basically a while loop. That's what an iterative server is,
  while (1)   {
    // Accept incoming connections
    struct sockaddr_in clientIPAddress;
    int s_size = sizeof(clientIPAddress);
    int slaveSocket = accept(masterSocket,
                             (struct sockaddr *)&clientIPAddress,
                             (socklen_t *)&s_size);

    if (slaveSocket < 0)
    {
      perror("accept");
      exit(-1);
    }

    // Process request.
    process_my_request(slaveSocket);

    // Close socket
    close(slaveSocket);
  }
}

void fork_server(int masterSocket) {
  while (1) {
    struct sockaddr_in clientIPAddress;
    int s_size = sizeof(clientIPAddress);
    int slaveSocket = accept(masterSocket, (struct sockaddr *)&clientIPAddress, (socklen_t *)&s_size);

    if (slaveSocket >= 0) {
      // call fork on this
      int ret = fork();
      if (ret == 0) {
        process_my_request(slaveSocket);
        close(slaveSocket);
        exit(0);
      }
      else {
        perror("FORK");
      }
      close(slaveSocket);
    }
  }
}

void request_thread(int masterSocket) {
  while (1) {
    struct sockaddr_in clientIPAddress;
    int s_size = sizeof(clientIPAddress);
    int slaveSocket = accept(masterSocket, (struct sockaddr *)&clientIPAddress, (socklen_t *)&s_size);
    if (slaveSocket >= 0)
    {
      pthread_t thrd;
      pthread_attr_t attr;
      pthread_attr_init(&attr);
      pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
      pthread_create(&thrd, &attr, (void *(*)(void *))process_my_request, (void *)slaveSocket);
    }
  }
}

void pool_of_threads(int masterSocket) {
  pthread_t threads[5];
  for (int i = 0; i < 4; i++) {
    pthread_create(&threads[i], NULL, (void *(*)(void *))loopthread, (void *)masterSocket);
  }
  loopthread(masterSocket);
}

void loopthread(int masterSocket) {
  while (1) {
    struct sockaddr_in clientIPAddress;
    int s_size = sizeof(clientIPAddress);
    pthread_mutex_lock(&mutex_m);
    int slaveSocket = accept(masterSocket, (struct sockaddr *)&clientIPAddress, (socklen_t *)&s_size);
    pthread_mutex_unlock(&mutex_m);
    if (slaveSocket >= 0) {
      process_my_request(slaveSocket);
    }
  }
}

void process_my_request(int fd) {
  // Buffer used to store the name received from the client
  const int MaxName = 1024 * 4;
  char name[ MaxName + 1 ];
  int nameLength = 0;
  int n;

  // Send prompt
  const char * prompt = "\nPut in your request:";
  //write( fd, prompt, strlen( prompt ) );

  // Currently character read
  
  unsigned char newChar = 0;

  // Last character read
  unsigned char lastChar = 0;
  unsigned char last2Char = 0;
  unsigned char last3Char = 0;
  //
  // The client should send <name><cr><lf>
  // Read the name of the client character by character until a
  // <CR><LF> is found.
  //
    
  while ( nameLength < MaxName && ( n = read( fd, &newChar, sizeof(newChar) ) ) > 0 ) {
    if (last3Char == '\015' && last2Char == '\012' && lastChar == '\015' && newChar == '\012') {
      // Discard previous <CR> from name
      nameLength--;
      break;
    }
    name[ nameLength ] = newChar;
    nameLength++;
    last3Char = last2Char;
    last2Char = lastChar;
    lastChar = newChar;
  }
  name[ nameLength ] = 0;
  printf("REQUEST = %s\n", name);

  string req(name);
  string cred = "Authorization: Basic ";
  const char *usrnm_pswd_64 = "amhzdTpwYXNzd29yZA==";
  cred.append(usrnm_pswd_64);
  int authIndex = req.find(cred);

  if (authIndex != std::string::npos) {
    int search = req.find("HTTP");
    string fileName = req.substr(4, search - 5);
    printf("file name = %s\n", fileName.c_str());
    if (fileName.compare("/") == 0)
    {
      fileName = "/index.html";
    }
    string path = "./http-root-dir/htdocs";
    path.append(fileName);

    string response = server_resp(path);
    printf("RESPONSE = %s\n", response.c_str());
    write(fd, response.c_str(), response.size());
  }
  else {
    
    write(fd, err_msg, strlen(err_msg));
  }

// tokenizing time
//   char * tokenizer = strtok(name, " ");
//   std::vector<std::string> commands; 
// //print out tokens
//   while (tokenizer) {
//     commands.push_back(std::string(tokenizer));
//     tokenizer = strtok(NULL, " ");
//   }
// if (verifyToken(commands) == true) {
// //  std::cout << "ha, it's true";
// } else {
// const char* testing = "HTTP/1.1 401 Unauthorized\015\012WWW-Authenticate: Basic realm=\"myhttpd-cs252\"\015\012Server: data.cs.purdue.edu\015\012Content-length: 5\015\012Content-Type: text/html\015\012\015\012Hello\015\012";
// write(fd, testing, strlen(testing));
// }

}

int verifyToken (std::vector<std::string> commands) {
  int counter = 0;
  for (auto & command : commands) {
    std::size_t found = command.find("Y3MyNTI6Y3MyNTI6cGFzc3dvcmQ=");
    if(found!=std::string::npos) {
//      fprintf(stderr, "found it");
      return true;
    }
    fprintf(stderr, "%d: %s\n\n", counter, command.c_str());
    counter++;
  }
//   for (int i=0; i<commands.size(); ++i) {
//  //   std::cout << commands[i];
//     std::cout << "index" + i;
//     std::cout << "\n";
//     if(commands[i] == "Authorization:BasicY3MyNTI6Y3MyNTI6cGFzc3dvcmQ=") {
//       std::cout << "returning true";
//       return true;
//     } 
//   }
return false; 
}

