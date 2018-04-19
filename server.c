/* A simple server which could process get requests using TCP
 * Implementation by Harrison Liu ID: 830859
 * For COMP30023 Computer Systems AS1 2018
 *
 *
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

//Gets a file
char *getfile(char *file_path, int *http_status, long *dsize);

//Replies to client
void http_reply(char *data, char *http_version, int http_status, int newsockfd,
                char *filetype, long size);

//Concats two strings
char *concat(const char *s1, const char *s2);

//Finds file type
char *get_type(char *token);

//Read the GET request
void read_msg(char *buffer, char *file_path, char *http_version);

//Create first line of return header based upon code
char *state(char *http_status);

//Used for multithreading and calling functions
void *run_thread(void *args);


typedef struct pt_data {
    int socket; //Socket file directory
    char *path; //Path to root
} pt_data;


#define STRING_LENGTH  256;
#define MAX_THREADS 6;
#define HTTP_RESPONSE 3;
#define TIMEOUT 200;


/*
 * The Main function takes in 2 command line inputs
 * these being
 * 1. Port number
 * 2. Root Directory
 *
 * */

int main(int argc, char **argv) {
    int sockfd, portno, timeout = 0;
    struct sockaddr_in serv_addr, cli_addr;
    socklen_t clilen;


    //The total number of threads which we can use
    int maxthreads = MAX_THREADS;
    //Time out variable, change if need to be longer
    int TIME_OUT = TIMEOUT;
    //Declare a dynamic array of pt_data structure types
    //Used to pass data into threads
    struct pt_data **data = malloc(sizeof(pt_data) * maxthreads);
    //Declare a dynamic array of threads
    pthread_t *thread = malloc(sizeof(pthread_t) * maxthreads);
    //Declare a dynamic array of socket locations.
    int n = 0, *sockets = malloc(sizeof(int) * maxthreads), i;
    //Pointer to hold command line argument of root path
    char *path;


    /*
     * Parts of the code are copied from sever.c from the LMS
     * These are just standard boilerplate code.
     * */

    if (argc < 2) {
        fprintf(stderr, "ERROR, no port provided\n");
        exit(1);
    }

    /* Create TCP socket */

    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0) {
        perror("ERROR opening socket");
        exit(1);
    }


    bzero((char *) &serv_addr, sizeof(serv_addr));

    portno = atoi(argv[1]);
    path = argv[2]; //Accquire the path to the root, no processing needed


    /* Create address we're going to listen on (given port number)
     - converted to network byte order & any IP address for
     this machine */

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);  // store in machine-neutral format

    /* Bind address to the socket */

    if (bind(sockfd, (struct sockaddr *) &serv_addr,
             sizeof(serv_addr)) < 0) {
        perror("ERROR on binding");
        exit(1);
    }

    /* Listen on socket - means we're ready to accept connections -
     incoming connection requests will be queued */

    listen(sockfd, 500);

    clilen = sizeof(cli_addr);

    /* Accept a connection - block until a connection is ready to
     be accepted. Get back a new file descriptor to communicate on. */

    int cnt = 0;



    /*
     * Begin of non boiler plate code
     *
     *
     * *******NOTE*********
     * Depends on how long the test
     * goes for, you may need to
     * configure the timeout varible at
     * the top, since at the current
     * moment, the server will automatically
     * close connection after 200 iterations
     * of the while loop
     * ********************
     *
     * */

    while (1) {

        timeout++; //Increment timeout value


        //For when all threads are currently being
        //Used or have been used
        if (cnt == maxthreads) {
            for (i = 0; i < n; i++) {
                //Free the thread data
                //And detech all threads
                pthread_detach(thread[n]);
                if (data[n] != NULL) {
                    free(data[n]->path);
                    free(data[n]);

                }
            }

            //When the timeout value has been exhausted
            //Free everything and close sockets
            if (timeout == TIME_OUT) {
                for (i = 0; i < n; i++) {
                    pthread_detach(thread[n]);
                    if (data[n] != NULL) {
                        free(data[n]->path);
                        free(data[n]);
                    }
                }
                break;
            }

            //Reset socket cnt and number of data points
            //Used
            cnt = 0;
            n = 0;

        }

        //Accept a connection
        sockets[cnt] = accept(sockfd, (struct sockaddr *) &cli_addr,
                              &clilen);

        if (sockets[cnt] < 0) {
            perror("ERROR on accept");
            exit(1);
        }


        //initialize the struct which will be passed into the
        //thread
        data[n] = malloc(sizeof(pt_data));
        data[n]->path = malloc(sizeof(char) * strlen(path));
        data[n]->path = strcpy(data[n]->path, path);
        data[n]->socket = sockets[cnt];

        //Create the thread
        pthread_create(&thread[n], NULL, run_thread, data[n]);

        //Increment cnt and m
        cnt++;
        n++;

    }

    //Terminate connection
    close(sockfd);

    return 0;
}

void *run_thread(void *args) {
    /*
     * Run thread reads from socket input
     * and returns a HTTP header with
     * a file.
     *
     * */
    int max_size = STRING_LENGTH;

    //Initialize the file path value
    char *file_path = malloc(sizeof(char) * max_size);
    //Initialize the http_version
    char *http_version = malloc(sizeof(char) * max_size);
    //Initialize the buffer size
    char *buffer = malloc(sizeof(char) * max_size);
    //Data is used to store the data
    //Token is used to split strings using strtok
    //tmp is also used in strtok to hold tempoary data
    //Filetype is used to store the version of the file
    char *data, *token, *tmp, *filetype;
    //Varible used in strtok
    const char s[2] = ".";
    //a interger which corresponds to the current HTTP status
    int http_status;
    //used to store the size of the file
    long size;
    //Iterator
    int n;

    //Assign the data structure a type
    pt_data *PT_DATA = (pt_data *) args;

    //Get socketfd
    int socket = PT_DATA->socket;

    //get path to root
    char *path = PT_DATA->path;


    //Initialize or reset buffer
    bzero(buffer, max_size);

    /* Read characters from the connection,
        then process */

    n = read(socket, buffer, max_size - 1);


    if (n < 0) {
        perror("ERROR reading from socket");
        exit(1);
    }

    //Read the message
    read_msg(buffer, file_path, http_version);
    //Get the file if possible
    data = getfile(concat(path, file_path), &http_status, &size);
    //Split the string
    token = strtok(file_path, s);
    //We want the last part of filepath, before
    //The null
    while (token != NULL) {
        tmp = token;
        token = strtok(NULL, s);
    }

    //Get the file type
    filetype = get_type(tmp);

    //Send off a reply
    http_reply(data, http_version, http_status, socket, filetype, size);

    //Close the fd.
    close(socket);



    //Free all thread specific values
    free(file_path);
    free(http_version);
    free(buffer);
    free(data);
    free(token);


    return NULL;
}


char *get_type(char *token) {

    //Function takes in a token and outputs a string
    //corresponding to filetype

    char *mime_html = "Content-Type: text/html";
    char *mime_jpeg = "Content-Type: image/jpeg";
    char *mime_css = "Content-Type: text/css";
    char *mime_javascript = "Content-Type: application/javascript";

    if (strcmp(token, "html") == 0) {
        return mime_html;
    }
    if (strcmp(token, "jpg") == 0) {
        return mime_jpeg;
    }
    if (strcmp(token, "css") == 0) {
        return mime_css;
    }
    if (strcmp(token, "js") == 0) {
        return mime_javascript;
    }
    return 0;
}


void read_msg(char *buffer, char *file_path, char *http_version) {

    int max = STRING_LENGTH;
    //Used to hold HTTP GET request
    //Only gets the first line in most cases
    char *tmp = malloc(sizeof(char) * max);
    //cnt is incremented as we read characters
    //cnt1 is used to load characters into memory locations
    //And resets after hitting a flag
    //flag is to indicate what we're reading right now
    int cnt = 0, cnt1 = 0, flag = 0;

    //while the buffer from the read message
    //hasn't been terminated
    while (buffer[cnt] != '\0') {

        //We hit a space!
        if (buffer[cnt] == ' ') {
            //Quickly terminate the tmp array
            tmp[cnt1] = '\0';

            //If we hit the file directory
            if (flag == 0) {
                //Do nothing

            }
            //If we are about to hit the HTTP version
            if (flag == 1) {
                //Copy the file path into another string
                strcpy(file_path, tmp);
            }
            //Set flag to increment
            flag++;
            //Reset tmp
            cnt1 = 0;
            //Go to next item in buffer array/string
            cnt++;
            continue;
        }
        //We have hit a newline, stop reading the buffer
        if (buffer[cnt] == '\n') {
            break;
        }
        //Add character to tmp array
        tmp[cnt1] = buffer[cnt];
        //increment everything
        cnt++;
        cnt1++;

    }
    //Get HTTP version since that is the last thing read
    //Before newline
    tmp[cnt1] = '\0';
    strcpy(http_version, tmp);
    //Free it.
    free(tmp);
}


char *getfile(char *file_path, int *http_status, long *dsize) {
    //File pointer for accessing file directory
    FILE *fp;
    //Cnt is used to read characters juan by juan
    int cnt = 0;
    //Size of the data
    long size;
    //Pointer to the data and a array use to read file
    char *data, c[0];

    //You must open in binary mode
    fp = fopen(file_path, "rb");
    if (fp == 0) {
        //File D.N.E
        *http_status = 404;
        return NULL;
    }
    //Seek to the end of the file to get the size
    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    //We will need to save the size for the http reply
    *dsize = size;
    fseek(fp, 0, SEEK_SET);


    //Dynamically allocate an array
    data = malloc(sizeof(char) * size);
    //Set HTTP status to we haz file
    *http_status = 200;

    //Assuming we haven't hit feof
    //we keep reading chaaracter by c haracter
    while (!feof(fp)) {
        fread(c, 1, 1, fp);
        data[cnt] = c[0];
        bzero(c, 1);
        cnt++;

        //There must be an issue with the file
        if (cnt > size + 10) {
            perror("ERROR, file likely corrupted");
            exit(1);
        }
    }
    //Terminator is a great movie
    data[cnt] = '\0';
    //Free the file path
    free(file_path);
    //Close this file
    fclose(fp);

    return data;
}


void http_reply(char *data, char *http_version, int http_status, int newsockfd,
                char *filetype, long size) {
    /*Function for HTTP reply
     * Takes a input of data, http_status, version,
     * socket file directory, file type and size of the data
     * outputs a http response
     * */
    int http_code_size = HTTP_RESPONSE;
    int max_size = STRING_LENGTH;


    int n;
    //Some buffers are size limited since there is no need for
    //It to be over 256 characters if you're only giving back a header
    char str[http_code_size], buffer[max_size], buffer1[max_size];


    //Convert HTTP_STATUS to a string
    sprintf(str, "%d", http_status);
    //Format the response header
    sprintf(buffer, "%s\n%s\n", state(str), filetype);
    //Append to final buffer
    strcpy(buffer1, buffer);


    //Given that data is not empty
    if (data != NULL) {
        //We will need a bigger dynamic array
        char *buffer2 = malloc(sizeof(char) * (strlen(buffer1) + size));
        if (buffer2 == NULL) {
            perror("ERROR failure to allocate memory");
            exit(1);
        }

        //Format buffer 1 into buffer2
        sprintf(buffer2, "%s\n", buffer1);
        //Using memcpy copy the memory location of the data onto
        //The memory location of buffer2+strlen(buffer1)
        memcpy(buffer2 + 1 + (int) strlen(buffer1), data, size);
        //send data
        n = write(newsockfd, buffer2, size + strlen(buffer1) + 1);
        //free the buffer
        free(buffer2);

    } else {
        //just print back the response header.
        n = write(newsockfd, buffer, strlen(buffer));
    }
    if (n < 0) {
        perror("ERROR writing to socket");
        exit(1);
    }
}

char *concat(const char *s1, const char *s2) {
    /*
     * Simple concatanation function from stack overflow
     * Attributed to David Heffernan
     *
     * */

    char *result = malloc(strlen(s1) + strlen(s2) + 1);
    strcpy(result, s1);
    strcat(result, s2);
    return result;
}

char *state(char *http_status) {
    /*
     * Hardcoded State return, takes http status string
     * and outputs a http request.
     *
     * */

    if (strcmp(http_status, "404") == 0) {
        return "HTTP/1.0 404";
    } else {
        return "HTTP/1.0 200 OK";
    }
}