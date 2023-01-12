#include "server.h"
#define PERM 0644

//Global Variables [Values Set in main()]

int queue_len           = INVALID;                              //Global integer to indicate the length of the queue
int cache_len           = INVALID;                              //Global integer to indicate the length or # of entries in the cache
int num_worker          = INVALID;                              //Global integer to indicate the number of worker threads
int num_dispatcher      = INVALID;                              //Global integer to indicate the number of dispatcher threads
FILE *logfile;                                                  //Global file pointer for writing to log file in worker
int cache_size=0;
/* ************************ Global Hints **********************************/
//int ????      = 0;                            //[Cache]           --> When using cache, how will you track which cache entry to evict from array?
int workerIndex = 0;                            //[worker()]        --> How will you track which index in the request queue to remove next?
int dispatcherIndex = 0;                        //[dispatcher()]    --> How will you know where to insert the next request received into the request queue?
int curequest= 0;                               //[multiple funct]  --> How will you update and utilize the current number of requests in the request queue?
int cache_order;
cache_entry_t *cache_arr;

pthread_t worker_thread[MAX_THREADS];           //[multiple funct]  --> How will you track the p_thread's that you create for workers?
pthread_t dispatcher_thread[MAX_THREADS];       //[multiple funct]  --> How will you track the p_thread's that you create for dispatchers?
int threadID[MAX_THREADS];                      //[multiple funct]  --> Might be helpful to track the ID's of your threads in a global array
int worker_threadID[MAX_THREADS]; 

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t req_lock = PTHREAD_MUTEX_INITIALIZER;          //What kind of locks will you need to make everything thread safe? [Hint you need multiple]
pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t some_content = PTHREAD_COND_INITIALIZER;  //What kind of CVs will you need  (i.e. queue full, queue empty) [Hint you need multiple]
pthread_cond_t req_queue_notfull = PTHREAD_COND_INITIALIZER;
pthread_cond_t req_queue_notempty = PTHREAD_COND_INITIALIZER;  
pthread_cond_t free_space = PTHREAD_COND_INITIALIZER;
request_t req_entries[MAX_QUEUE_LEN];                    //How will you track the requests globally between threads? How will you ensure this is thread safe?


//cache_entry_t* ?????;                                  //[Cache]  --> How will you read from, add to, etc. the cache? Likely want this to be global

/**********************************************************************************/


/*
  THE CODE STRUCTURE GIVEN BELOW IS JUST A SUGGESTION. FEEL FREE TO MODIFY AS NEEDED
*/


/* ******************************** Cache Code  ***********************************/

// Function to check whether the given request is present in cache
int getCacheIndex(char *request){
  for (int i = 0; i<cache_size; i ++){
    if(strcmp(request,cache_arr[i].request)){
      return i;
    }
  }
  /* TODO (GET CACHE INDEX)
  *    Description:      return the index if the request is present in the cache otherwise return INVALID
  */
  return INVALID;
}

// Function to add the request and its file content into the cache
void addIntoCache(char *mybuf, char *memory , int memory_size){
  if (cache_order > cache_len){
    cache_order-= cache_len;
  }
  if((cache_arr[cache_order].request)!=NULL&&cache_arr[cache_order].content!=NULL){
    free(cache_arr[cache_order].request);
    free(cache_arr[cache_order].content);
  }

  cache_entry_t *new_entry = (cache_entry_t *) malloc(sizeof(cache_entry_t));
  cache_arr[cache_order].len = memory_size;
  cache_arr[cache_order].request = malloc(sizeof(mybuf));
  strcpy((cache_arr[cache_order].request),mybuf);
  cache_arr[cache_order].content = malloc(sizeof(memory));
  strcpy((cache_arr[cache_order].content),memory);
  printf("cache_order: %i \n",new_entry->len);
  printf("request: %s \n",(cache_arr[cache_order].request));
  printf("content: %s \n",(cache_arr[cache_order].content));
  cache_order ++;
  
  /* TODO (ADD CACHE)
  *    Description:      It should add the request at an index according to the cache replacement policy
  *                      Make sure to allocate/free memory when adding or replacing cache entries
  */
}

// Function to clear the memory allocated to the cache
void deleteCache(){
  for(int i =0; i<cache_size; i++){
    free(cache_arr[i].request);
    free(cache_arr[i].content);
  }
  free(cache_arr);
  cache_order = 0;
  cache_size = 0;
  /* TODO (CACHE)
  *    Description:      De-allocate/free the cache memory
  */
}

// Function to initialize the cache
void initCache(){
  cache_arr = malloc(sizeof(cache_entry_t)*cache_len);
  cache_order = 0;
  cache_size = 0;
  /* TODO (CACHE)
  *    Description:      Allocate and initialize an array of cache entries of length cache size
  */
}

/**********************************************************************************/

/* ************************************ Utilities ********************************/
// Function to get the content type from the request
char* getContentType(char *mybuf) {
  /* TODO (Get Content Type)
  *    Description:      Should return the content type based on the file type in the request
  *                      (See Section 5 in Project description for more details)
  *    Hint:             Need to check the end of the string passed in to check for .html, .jpg, .gif, etc.
  */
  char * content_type="text/plain";
  char *ext = strrchr(mybuf, '.');
  if (strcmp(ext, ".html")==0){
    content_type="text/html";
  }
  else if (strcmp(ext, ".htm")==0){
    content_type="text/html";
  }
  else if (strcmp(ext, ".jpg")==0){
    content_type="image/jpeg";
  }
  else if (strcmp(ext, ".gif")==0){
    content_type="image/gif";
  }
  return content_type;
}

// Function to open and read the file from the disk into the memory. Add necessary arguments as needed
// Hint: caller must malloc the memory space
int readFromDisk(int fd, char *mybuf, void **memory) {
  //    Description: Try and open requested file, return INVALID if you cannot meaning error
  FILE *fp;
  int file_Size =-1;
  char cwd[512];
 // printf("cwd %s\n",getcwd(cwd,sizeof(cwd)));
 getcwd(cwd,sizeof(cwd));
  strcat(cwd, mybuf);
  mybuf=cwd;
 // printf("full location %s\n",mybuf );
  if((fp = fopen(mybuf, "r")) == NULL){
     fprintf (stderr, "ERROR: Fail to open the file.\n");
    return INVALID;
  }
 // printf("are we getting here\n");
  fseek(fp,0,SEEK_END);  //seek to end of file
  file_Size = ftell(fp);  //count how large the file is
  //printf("how big file %d\n",file_Size);
  //printf("do we get before the malloc %d\n",file_Size);
  fseek(fp,0,SEEK_SET);  //back to the start of the file to read; 
  *memory = malloc(file_Size);
//  printf("do we get past the malloc  %d\n",file_Size);
  fread(*memory,1, file_Size, fp);
 // printf("do we get here 10000?  %d\n",file_Size);
  fclose(fp);
  
 // fprintf (stderr,"The requested file path is: %s\n", mybuf);
  return file_Size;

  /* TODO
  *    Description:      Find the size of the file you need to read, read all of the contents into a memory location and return the file size
  *    Hint:             Using fstat or fseek could be helpful here
  *                      What do we do with files after we open them?
  */
 //ACTUAL TO DO 
 //WE NEED TO ADD THE MEMORY WHICH THEY EXPLAIN HOW TO DO IN THE FAQ AND THEN COPY THE CONTENTS OF THE FILE INTO THAT MEMORY LOCATION

/*struct stat buffer; 
int         status;
status = stat(mybuf, &buffer);
  printf("%ld\n",buffer.st_size);

  //TODO remove this line and follow directions above
*/}

/**********************************************************************************/

// Function to receive the path)request from the client and add to the queue
void * dispatch(void *arg) {

  /********************* DO NOT REMOVE SECTION - TOP     *********************/


  /* TODO (B.I)
  *    Description:      Get the id as an input argument from arg, set it to ID
  
  */
 int num_request=*(int *) arg;
  while (1) {

    /* TODO (FOR INTERMEDIATE SUBMISSION)
    *    Description:      Receive a single request and print the conents of that request
    *                      The TODO's below are for the full submission, you do not have to use a
    *                      buffer to receive a single request
    *    Hint:             Helpful Functions: int accept_connection(void) | int get_request(int fd, char *filename
    *                      Recommend using the request_t structure from server.h to store the request. (Refer section 15 on the project write up)
    */



    /* TODO (B.II)
    *    Description:      Accept client connection
    *    Utility Function: int accept_connection(void) //utils.h => Line 24
    */

      int fd = accept_connection();


    /* TODO (B.III)
    *    Description:      Get request from the client
    *    Utility Function: int get_request(int fd, char *filename); //utils.h => Line 41
    */
    char buffer[1024];
      int request= get_request(fd,buffer);


      fprintf(stderr, "Dispatcher Received Request: fd[%d] request[%s]\n", fd, buffer);
    /* TODO (B.IV)
    *    Description:      Add the request into the queue
    */

        //(1) Copy the filename from get_request into allocated memory to put on request queue
        request_t *req=(request_t *) malloc(sizeof(request_t));
        req->fd=fd;
        req->request=buffer;

        //(2) Request thread safe access to the request queue
         pthread_mutex_lock(&req_lock);
        //(3) Check for a full queue... wait for an empty one which is signaled from req_queue_notfull
         while(curequest==MAX_QUEUE_LEN){
           pthread_cond_wait(&req_queue_notfull, &req_lock);
}
        //(4) Insert the request into the queue
       
          req_entries[curequest]=*req;
        //(5) Update the queue index in a circular fashion
          if(curequest==MAX_QUEUE_LEN){
             curequest = 0;
          }
          else{
            curequest++;
          }
        //(6) Release the lock on the request queue and signal that the queue is not empty anymore
       
        pthread_cond_signal(&req_queue_notempty);		
		pthread_mutex_unlock(&req_lock);
 }

  return NULL;
}

/**********************************************************************************/
// Function to retrieve the request from the queue, process it and then return a result to the client
void * worker(void *arg) {
  /********************* DO NOT REMOVE SECTION - BOTTOM      *********************/


 


  // Helpful/Suggested Declarations
  int num_request = 0;                                    //Integer for tracking each request for printing into the log
  bool cache_hit  = false;                                //Boolean flag for tracking cache hits or misses if doing
  int filesize    = 0;                                    //Integer for holding the file size returned from readFromDisk or the cache
  void *memory    = NULL;                                 //memory pointer where contents being requested are read and stored
  int fd          = INVALID;                              //Integer to hold the file descriptor of incoming request
  char mybuf[BUFF_SIZE];                                  //String to hold the file path from the request




  /* TODO (C.I)
  *    Description:      Get the id as an input argument from arg, set it to ID
  */
 int id =*(int *) arg;

//printf("number request %d\n",num_request);
  while (1) {
    /* TODO (C.II)
    *    Description:      Get the request from the queue and do as follows
    */
          //(1) Request thread safe access to the request queue by getting the req_queue_mutex lock
      pthread_mutex_lock(&req_lock);
          //(2) While the request queue is empty conditionally wait for the request queue lock once the not empty signal is raised
          while (curequest==0) {
			//Buffer empty, release lock and sleep until buffer not empty
	//		printf("Consumer: Empty Buffer, sleeping until buffer_not_empty condition signal\n");
			pthread_cond_wait(&req_queue_notempty, &req_lock);
		}
    request_t req;
          //(3) Now that you have the lock AND the queue is not empty, read from the request queue
          if (curequest==0){
            req=req_entries[MAX_QUEUE_LEN];
            num_request=MAX_QUEUE_LEN;
            curequest=MAX_QUEUE_LEN;
          }
          else{
           req=req_entries[curequest-1];
             num_request=curequest-1;
            //(4) Update the request queue remove index in a circular fashion
            curequest--;
            }

          //(5) Check for a path with only a "/" if that is the case add index.html to it
          if(strcmp(req.request,"/")==0){
            req.request="/index.html";
          }
          strcpy(mybuf,req.request);
          fd=req.fd;
          //(6) Fire the request queue not full signal to indicate the queue has a slot opened up and release the request queue lock
          pthread_cond_signal(&req_queue_notfull);		
		    pthread_mutex_unlock(&req_lock);
    /* TODO (C.III)
    *    Description:      Get the data from the disk or the cache
    *    Local Function:   int readFromDisk(//necessary arguments//);
    *                      int getCacheIndex(char *request);
    *                      void addIntoCache(char *mybuf, char *memory , int memory_size);
    */
    int cache_results = getCacheIndex(mybuf);
    if(cache_results!=INVALID){
      cache_hit = true; 

    }else{
      filesize = readFromDisk(fd,mybuf,&memory);
      addIntoCache(mybuf,memory,filesize); //THIS IS IF YOU GET CACHE WORKING
    }
  //CHECK IF IN CACHE THEN USE CACHE INSTEAD
  //ADD IT TO THE CACHE
  
    /* TODO (C.IV)
    *    Description:      Log the request into the file and terminal
    *    Utility Function: LogPrettyPrint(FILE* to_write, int threadId, int requestNumber, int file_descriptor, char* request_str, int num_bytes_or_error, bool cache_hit);
    *    Hint:             Call LogPrettyPrint with to_write = NULL which will print to the terminal
    *                      You will need to lock and unlock the logfile to write to it in a thread safe manor
    */
    pthread_mutex_lock(&log_lock);
    LogPrettyPrint(NULL, id, num_request, fd, mybuf, filesize,cache_hit);
    LogPrettyPrint(logfile , id, num_request, fd, mybuf, filesize,cache_hit);
    pthread_mutex_unlock(&log_lock);
    /* TODO (C.V)
    *    Description:      Get the content type and return the result or error
    *    Utility Function: (1) int return_result(int fd, char *content_type, char *buf, int numbytes); //look in utils.h
    *                      (2) int return_error(int fd, char *buf); //look in utils.h
    */
    char* TypeOfContent = getContentType(mybuf);
    
    //printf("%s\n",TypeOfContent);
    if(strcmp(TypeOfContent,"text/plain")==0 || strcmp(TypeOfContent,"text/html") ==0 || strcmp(TypeOfContent,"image/jpeg")==0||strcmp(TypeOfContent,"image/gif")==0){
     // printf("do we get in if\n");
      return_result(fd,TypeOfContent,memory,filesize);
     // printf("after return result\n");
      //free(memory);
    }else{
     // printf("do we get out of if\n");
      return_error(fd,memory);
      //printf("after return result\n");
      //free(memory);
    }
    
  }

//printf("after return result\n");


  return NULL;

}

/**********************************************************************************/

int main(int argc, char **argv) {

  /********************* Dreturn resulfO NOT REMOVE SECTION - TOP     *********************/
  // Error check on number of arguments

  if(argc != 7){
    printf("usage: %s port path num_dispatcher num_workers queue_length cache_size\n", argv[0]);
    return -1;
  }

  int port            = -1;
  char path[PATH_MAX] = "no path set\0";
  num_dispatcher      = -1;                               //global variable
  num_worker          = -1;                               //global variable
  queue_len           = -1;                               //global variable
  cache_len           = -1;                               //global variable


  /********************* DO NOT REMOVE SECTION - BOTTOM  *********************/
  /* TODO (A.I)
  *    Description:      Get the input args --> (1) port (2) path (3) num_dispatcher (4) num_workers  (5) queue_length (6) cache_size
  */
  port=atoi(argv[1]);
  strcpy(path,argv[2]);
  num_dispatcher=atoi(argv[3]);
  num_worker =atoi(argv[4]);
  queue_len = atoi(argv[5]);
  cache_len=atoi(argv[6]);
 //TODO REMOVE LINES ABOVE 
 
  if(port < MIN_PORT || port >MAX_PORT){
       printf("BAD PORT\n");
       return;
  }
  //if()   //checking path
  if(num_dispatcher<1 || num_dispatcher > MAX_THREADS){
    printf("BAD DISPATCHERS\n");
       return;
  }
  if(num_worker<1 || num_worker>MAX_THREADS){
    printf("BAD WORKERS\n");
       return;
  }
  if(queue_len< 1 || queue_len >MAX_QUEUE_LEN){
    printf("BAD queue\n");
       return;
  }
  if(cache_len<1|| cache_len>MAX_CE){
  printf("BAD CACHE\n");
       return;
  }
  initCache();



  /* TODO (A.II)
  *    Description:     Perform error checks on the input arguments
  *    Hints:           (1) port: {Should be >= MIN_PORT and <= MAX_PORT} | (2) path: {Consider checking if path exists (or will be caught later)}
  *                     (3) num_dispatcher: {Should be >= 1 and <= MAX_THREADS} | (4) num_workers: {Should be >= 1 and <= MAX_THREADS}
  *                     (5) queue_length: {Should be >= 1 and <= MAX_QUEUE_LEN} | (6) cache_size: {Should be >= 1 and <= MAX_CE}
  */



  /********************* DO NOT REMOVE SECTION - TOP    *********************/
  printf("Arguments Verified:\n\
    Port:           [%d]\n\
    Path:           [%s]\n\
    num_dispatcher: [%d]\n\
    num_workers:    [%d]\n\
    queue_length:   [%d]\n\
    cache_size:     [%d]\n\n", port, path, num_dispatcher, num_worker, queue_len, cache_len);
  /********************* DO NOT REMOVE SECTION - BOTTOM  *********************/


  /* TODO (A.III)
  *    Description:      Open log file
  *    Hint:             Use Global "File* logfile", use "web_server_log" as the name, what open flags do you want?
  */
  logfile=fopen("web_server_log","w");


  /* TODO (A.IV)
  *    Description:      Change the current working directory to server root directory
  *    Hint:             Check for error!
  */
 //chdir("/home/dolli028/Documents/csci-4061-fall-22/4061-group-95/project_3_posted/testing/");
    
    if(chdir(path)==-1){
        printf("BAD DIRECTORY PATH\n");
    }
    
  /* TODO (A.V)
  *    Description:      Initialize cache
  *    Local Function:   void    initCache();
  */
initCache();


  /* TODO (A.VI)
  *    Description:      Start the server
  *    Utility Function: void init(int port); //look in utils.h
  */

init(port);

  /* TODO (A.VII)
  *    Description:      Create dispatcher and worker threads
  *    Hints:            Use pthread_create, you will want to store pthread's globally
  *                      You will want to initialize some kind of global array to pass in thread ID's
  *                      How should you track this p_thread so you can terminate it later? [global]
  */
 
    for( int  i=0; i<num_dispatcher;i++){
      threadID[i]=i;
      if(pthread_create(&(dispatcher_thread[i]), NULL, dispatch, (void *) &threadID[i]) != 0) {
            //printf("Dispatcher Thread %d failed to create\n",i);
        }
       // printf("Dispatcher Thread created successfully: %d\n",i);
      }
        for( int  i=0; i<num_worker;i++){
          worker_threadID[i]=i;
         // printf("id number %d\n",worker_threadID[i]);
      if(pthread_create(&(worker_thread[i]), NULL, worker, (void *) &worker_threadID[i]) != 0) { //can you use threadid again
            //printf("Worker Thread %d failed to create\n",i);
        }
      //  printf("Worker Thread created successfully: %d\n",i);
      }


  // Wait for each of the threads to complete their work
  // Threads (if created) will not exit (see while loop), but this keeps main from exiting
  int i;
  for(i = 0; i < num_worker; i++){
    fprintf(stderr, "JOINING WORKER %d \n",i);
    if((pthread_join(worker_thread[i], NULL)) != 0){
      printf("ERROR : Fail to join worker thread %d.\n", i);
    }
  }
  for(i = 0; i < num_dispatcher; i++){
    fprintf(stderr, "JOINING DISPATCHER %d \n",i);
    if((pthread_join(dispatcher_thread[i], NULL)) != 0){
      printf("ERROR : Fail to join dispatcher thread %d.\n", i);
    }
  }
  fprintf(stderr, "SERVER DONE \n");  // will never be reached in SOLUTION
}
