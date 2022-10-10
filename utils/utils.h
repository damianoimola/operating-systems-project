#pragma once

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <limits.h>
#include <stdbool.h>
#include <math.h>
#include <regex.h>
#include <termios.h>


#define CODE_SIZE 10
#define IP_ADDR_SIZE 16
#define DEFAULT_PORT 4444
#define MAX_INPUT_SIZE 255
#define WANT_TO_BOOK 1
#define WANT_TO_CANCEL 2
#define WANT_TO_SIGN_IN 1
#define WANT_TO_SIGN_UP 2
#define WANT_TO_EXIT 3


extern long get_long(char * msg){
    long a;
    char buf[1024]; // use 1KiB just to be sure
    int success; // flag for successful conversion
    do {
        
        puts(msg);
        //fflush(stdout);
        
        if (!fgets(buf, 1024, stdin)) {
            // reading input failed:
            return 1;
        }
        
        // have some input, convert it to integer:
        char *endptr;
        errno = 0; // reset error number
        a = strtol(buf, &endptr, 10);
        
        
        if (errno == ERANGE) {
            //printf("Sorry, this number is too small or too large.\n");
            success = 0;
        } else if (endptr == buf) {
            // no character was read
            success = 0;
        } else if (*endptr && *endptr != '\n') {
            // *endptr is neither end of string nor newline,
            // so we didn't convert the *whole* input
            success = 0;
        } else {
            success = 1;
        }
        
        if(success == 0){
            printf("Invalid input.\n");
            fflush(stdout);
        }
        
    } while (!success); // repeat until we got a valid number
    
    return a;
}



extern char * get_string(char *msg, ssize_t size){
    char *string = malloc(size * sizeof(char));
    
    puts(msg);
    
    if(fgets(string, size, stdin))
        string[strcspn(string, "\n ")] = '\0';
    
    return string;
}


void get_password(char password[])
{
    static struct termios oldt, newt;
    int i = 0;
    int c;

    /*saving the old settings of STDIN_FILENO and copy settings for resetting*/
    tcgetattr( STDIN_FILENO, &oldt);
    newt = oldt;

    /*setting the approriate bit in the termios struct*/
    newt.c_lflag &= ~(ECHO);          

    /*setting the new bits*/
    tcsetattr( STDIN_FILENO, TCSANOW, &newt);

    /*reading the password from the console*/
    while ((c = getchar())!= '\n' && c != EOF && i < MAX_INPUT_SIZE){
        password[i++] = c;
    }
    password[i] = '\0';

    /*resetting our old STDIN_FILENO*/ 
    tcsetattr( STDIN_FILENO, TCSANOW, &oldt);

}
