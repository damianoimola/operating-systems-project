#include "../utils/utils.h"

#define BACKLOG 10
#define MSG_SIZE 32
#define LOCALHOST "127.0.0.1"
#define TIMEOUT 180



// method signatures
void send0();
void receive3();
bool want_cancel();
void user_access();
void setup_events();
int receive1(int *n, int *m);
void check_semaphore_state();
void get_user_info(long choice);
long get_port(int argc, char *argv[]);
bool check_email_format(char *email);
bool send2(int n, int m, int seats_free);
char *get_address(int argc, char *argv[]);
char *check_address_format(char *address);
void startup_connection(long port, char *address);



// variabili globali
int conn_s = -2;                    // connection socket descriptor
bool communicated = true;           // to check if the booking management went properly



#define error(msg) {\
            fprintf(stderr, msg);\
            printf("\nError -> %s\n", strerror(errno));\
            if((conn_s >= 0) && (close(conn_s) < 0)) { perror("closing connection error"); }\
            exit(EXIT_FAILURE);\
}\



void event_handler(int signal){
    printf("segnale ricevuto %d\n", signal);
    if(signal == SIGALRM)
        puts("\nTimeout elapsed");
    else if(signal == SIGUSR1)
        puts("\nCommunication interrupted");
    else if(communicated == false)
        puts("\nA communication error has occurred");
    
    
    // to handle when connection is opened and "close" syscall failed
    if((conn_s >= 0) && (close(conn_s) < 0))
        puts("connection closing failed.");
    
    exit(EXIT_FAILURE);
}


int main(int argc, char *argv[]){
    int n;              // # rows
    int m;              // # cols
    long port;          // port number
    char *address;      // ip address
    int seats_free;     // number of seats available
    
    // retieve input's information
    port = get_port(argc, argv);
    address = get_address(argc, argv);
    
    // handling events
    setup_events();
    
    // initializing communication components
    startup_connection(port, address);
    
    // handle the access of user
    user_access();
    
    // handling the canceletion of the booking
    if(want_cancel() == false){
        seats_free = receive1(&n, &m);
        
        if(send2(n, m, seats_free))
            receive3();
        else
            // cancealing timer, because user doesn't
            // want insert any other seat (no "receive3")
            alarm(0);
        
    } else {
        send0();
    }
    
    close(conn_s);
}


void setup_events(){
    
    sigset_t set;
    struct sigaction act;

    sigfillset(&set);

    //act.sa_sigaction = event_handler; 
    act.sa_handler = event_handler; 
    act.sa_mask =  set;
    act.sa_flags = SA_SIGINFO;
    act.sa_restorer = NULL;
    
    // CATCHED signals
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGHUP, &act, NULL);
    sigaction(SIGTERM, &act, NULL);
    sigaction(SIGQUIT, &act, NULL);
    sigaction(SIGALRM, &act, NULL);
    sigaction(SIGPIPE, &act, NULL);
    // because illegal instructions come from outsiders
    sigaction(SIGILL, &act, NULL);
    // because segmentation errors can be forced bringing undefined behaviours
    sigaction(SIGSEGV, &act, NULL);
    // because it's usefull to handle '0' seats booked
    sigaction(SIGUSR1, &act, NULL);
    
    
    // IGNORED signals
    // because there are no forks
    signal(SIGCHLD, SIG_IGN);
    // because we don't define a meaning for both of them
    signal(SIGUSR2, SIG_IGN);
}


// starting up the connection with the server
void startup_connection(long port, char *address){
    struct sockaddr_in servaddr;  // server address structure
    struct hostent *he = NULL;    // host entity

    // creating socket
    if ((conn_s = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        error("client: error during socket's creation.\n");


    // resetting serveraddr structure   
    // and fill it with relevant data
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);

    printf("Connecting to server ip: %s, at port %lu\n", address, port);


    // set remote IP address
    if (inet_aton(address, &servaddr.sin_addr) <= 0 )
    {
        printf("client: invalid IP address\nclient: name resolution...");
        
        if ((he = gethostbyname(address)) == NULL)
            error("client: host resolution failed");
        
        printf("client: host resolution succeded.\n\n");
        
        // assigning to the struct the official host's name
        servaddr.sin_addr = *((struct in_addr *) he->h_addr);
    }

    // connecting to the server addr
    if (connect(conn_s, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0)
        error("client: error during connect.\n");
    
    // set timeout timer for receive and send data
    struct timeval send_timeout;      
    send_timeout.tv_sec = 30; // Number of whole seconds of elapsed time
    send_timeout.tv_usec = 0; // Number of microseconds of rest of elapsed time minus tv_sec. Always less than one million
    
    if (setsockopt (conn_s, SOL_SOCKET, SO_RCVTIMEO, &send_timeout, sizeof(send_timeout)) < 0)
        error("setsockopt failed\n");
    
    
    struct timeval recv_timeout;      
    recv_timeout.tv_sec = 5; // Number of whole seconds of elapsed time
    recv_timeout.tv_usec = 0; // Number of microseconds of rest of elapsed time minus tv_sec. Always less than one million
    
    if (setsockopt (conn_s, SOL_SOCKET, SO_SNDTIMEO, &recv_timeout, sizeof(recv_timeout)) < 0)
        error("setsockopt failed\n");
}


void get_user_info(long choice){
    char *email;
    char *username; 
    char password[MAX_INPUT_SIZE];
    char server_answer = '1';
    int res;
    
    do{
        system("clear");
        
        if(server_answer != '1')
            puts("username o password errati/utente già esistente.");
        
        // EMAIL
        do{
            email = get_string("e-mail: ", MAX_INPUT_SIZE);
        }while(check_email_format(email) == false);
        
        // sending the e-mail to the server
        if((write(conn_s, email, MAX_INPUT_SIZE)) == -1)                                                // write -2.1.1
            error("write -2.1.1 failed");
        
        
        
        
        // NICKNAME
        if(choice == WANT_TO_SIGN_UP){
            username = get_string("username: ", MAX_INPUT_SIZE);
            
            // sending the username to the server
            if((write(conn_s, username, MAX_INPUT_SIZE)) == -1)                                         // write -2.1.2
                error("write -2.1.2 failed");
        }
            
            
            
        // PASSWORD
        do {
            puts("password: ");
            get_password(password);
        } while(strlen(password) < 8 && strlen(password) > MAX_INPUT_SIZE);
        
        // sending the password to the server
        if((write(conn_s, password, MAX_INPUT_SIZE)) == -1)                                             // write -2.1.3
            error("write -2.1.3 failed");
        
        
        res = read(conn_s, &server_answer, sizeof(char));                                               // read -2.2

        if(res == -1){
            error("read -2.2 failed.");
        } else if(res == 0)
            raise(SIGINT);
        
        
        if(server_answer == '1' && choice == WANT_TO_SIGN_IN){
            if((username = malloc(sizeof(char) * MAX_INPUT_SIZE)) == NULL)
                error("memory allocation failed.");
            
            res = read(conn_s, username, MAX_INPUT_SIZE * sizeof(char));                                               // read -2.3

            if(res == -1){
                error("read -2.3 failed.");
            } else if(res == 0)
                raise(SIGINT);
        }
        
        
    }while(server_answer != '1');
    
    system("clear");
    
    printf("Welcome %s\n", username);
    fflush(stdout);
}


void user_access(){
    long choice;
    char *message = "What do you want to do?\n\n\t1) Sign in\n\t2) Sign up\n\t3) Exit\nEnter a code: ";
    bool flag = false;                                          // to handle the user menu retry
    
    do{
        choice = get_long(message);
    
        switch(choice){
            case 1: //Sign in
                // sending the choice to the server
                if((write(conn_s, "1", sizeof(char))) == -1)                                     // write -2
                    error("write -2 failed");
                flag = true;
                break; 
                
            case 2: //Sign Up
                // sending the choice to the server
                if((write(conn_s, "2", sizeof(char))) == -1)                                     // write -2
                    error("write -2 failed");
                flag = true;
                break;
                
            case 3:
                // sending the choice to the server
                if((write(conn_s, "3", sizeof(char))) == -1)                                     // write -2
                    error("write -2 failed");
                
                // we are closing the connection because otherwise
                // there would be a pending connected socket
                close(conn_s);
                exit(EXIT_SUCCESS);
                
            default:
                system("clear");
                puts("Warning, invalid input");
                break;
        }
        
        get_user_info(choice);
    } while(flag == false);
}


bool want_cancel(){
    long choice;
    char *welcome_message = "Welcome, what do you want to do?\n\n\t1) Book seats\n\t2) Cancel booking\n\t3) Exit\nEnter a code: ";
    
    do{
        choice = get_long(welcome_message);
    
        switch(choice){
            case 1:
                // sending the choice to the server
                if((write(conn_s, "1", sizeof(char))) == -1)                                     // write -1.1
                    error("write -1.1 failed");
                
                return false;
                
            case 2:
                // sending the choice to the server
                if((write(conn_s, "2", sizeof(char))) == -1)                                     // write -1.2
                    error("write -1.2 failed");
                
                return true;
                
            case 3:
                // sending the choice to the server
                if((write(conn_s, "3", sizeof(char))) == -1)                                     // write -1.3
                    error("write -1.3 failed");
                
                // we are closing the connection because otherwise
                // there would be a pending connected socket
                close(conn_s);
                exit(EXIT_SUCCESS);
                
            default:
                system("clear");
                puts("Warning, invalid input");
                break;
        }
    } while(true);
}


void send0(){
    long code;
    char *buff;
    int res;
    
    if((buff = malloc((CODE_SIZE + 1) * sizeof(char))) == NULL)
        error("memory allocation failed");
    
    while((code = get_long("Enter booking code: ")) > INT_MAX && code < 1000000000)
        puts("Warning, invalid code.");
    
    
    snprintf(buff, (CODE_SIZE + 1) * sizeof(char), "%lu", code);
    
#ifdef DEBUG
    printf("sending %s to server\n", buff);
#endif
    
    // sending the code to the server
    if((write(conn_s, buff, CODE_SIZE * sizeof(char))) == -1)                                               // write 0.1
        error("write 0.1 failed");
    
    // reading the answer from the server
    res = read(conn_s, buff, sizeof(char));                                                              // read 0.2
    
    if(res == -1){
        error("read 0.2 failed.");
    } else if(res == 0)
        raise(SIGUSR1);
    
    
    
    if(buff[0] == '1')
        puts("Cancellation succeded");
    else if(buff[0] == '0')
        puts("Error during cancellation's process");
        
}


// receiving seats availabilty from server
int receive1(int *n, int *m){
    char *buff;                                     // temporary buffer
    ssize_t seat_len;                               // max # of digits for seats number
    char *temp;                                     // temporary buffer for printing seats map with padding
    int seats_free = 0;                             // number of available seats
    int res;
    
    
    // reading n
    if((buff = malloc(sizeof(int) + sizeof(char))) == NULL)
        error("memory allocation failed.");
    
    res = read(conn_s, buff, sizeof(int));            // read 1
    
    if(res == -1){
        error("read 1 failed.");
    } else if(res == 0)
        raise(SIGUSR1);
    
    buff[sizeof(int)] = '\0';
    *n = *(int *)buff;
    
    
    
    
    // reading m
    res = read(conn_s, buff, sizeof(int));            // read 2
    
    if(res == -1){
        error("read 2 failed.");
    } else if(res == 0)
        raise(SIGUSR1);
    
    buff[sizeof(int)] = '\0';
    *m = *(int *)buff;
    
    
    
    if((buff = realloc(buff, (*m)*sizeof(char))) == NULL)
        error("memory re-allocation failed.");
    
#ifdef DEBUG
    printf("rows: %d, cols: %d\n", *n, *m);
#endif
    
    
    // printing the seats map
    seat_len = log10((*n)*(*m)) + 2; // n*m = total seats
    
    if((temp = malloc(seat_len)) == NULL)
        error("memory allocation failed.");
    
    for(int i=0; i<seat_len-1; i++){
        temp[i] = ' ';
    }
    temp[seat_len-1] = '\0';
    
    
    // check for the round
    check_semaphore_state(conn_s);
    
    
    // trigger the alarm timer: not needed in "send0" method
    // since it doesn't block any other client
    alarm(TIMEOUT);
    
    
    puts("");
    
    for(int i=0; i<(*n); i++){
        // reads one complete row from server
        res = read(conn_s, buff, (*m)*sizeof(char));               // read 3
    
        if(res == -1){
            error("read 3 failed.");
        } else if(res == 0)
            raise(SIGUSR1);
        
        
        printf("ROW -> %d\t", i + 1);
        
        for(int j=0; j<(*m); j++){
            int current = i*(*m)+j+1;
            ssize_t current_seat_len = log10(current) + 1;
            char *string;
            
            
            if(buff[j] == '0'){
                seats_free++;
                
                // printing the current number and the trailing padding
                if((string = malloc(current_seat_len + 1)) == NULL)
                    error("memory allocation failed");
                
                snprintf(string, current_seat_len + 1, "%d", current);
                memcpy(temp, string, current_seat_len);
            } else {
                // resetting temp variable
                for(int i=0; i<seat_len-1; i++){
                    temp[i] = ' ';
                }
                temp[seat_len-1] = '\0';
                
                // printing 'X' instead of the current number
                if((string = malloc(2 * sizeof(char))) == NULL)
                    error("memory allocation failed");
                
                snprintf(string, 2, "%s", "X");
                memcpy(temp, string, 1);
                
                printf("\033[0;31m");   // set color to red
            }
            
            
            printf("[%s] ", temp);
            printf("\033[97m");         // reset color to white
            fflush(stdout);
            free(string);
        }
        puts("");
    }
    
    free(buff);
    free(temp);
    
    
    // no seats available
    if(seats_free == 0){
        printf("\033[0;31m");                       // set color to red
        printf("WARNING: no seats available.\n");
        printf("\033[97m");                         // reset color to white
        
        raise(SIGINT);
    }
    
    // it is initialized to true, because if there are no seats available
    // the handler of signals doesn't drop a "comunication error", but it
    // has to do it at this point
    communicated = false;
    
    return seats_free;
}


// sending the seats choice to the server
bool send2(int n, int m, int seats_free){
    char *redundancy;                                           // flag to check about seats booked multiple times
    char *buff;                                                 // temporary array
    char *msg;                                                  // variable to handle parametrized messages
    int seat;                                                   // used for user input, stores the seat
    int res;
    long bookings;                                              // # seats to book
    ssize_t book_size;                                          // # of digits of bookings
    ssize_t seat_size = (log10(n*m) + 2) * sizeof(char);        // # of max digit of seat number
    
    
    // retrieving # of seats
    do{
        bookings = get_long("\nEnter the number of seats to book ('0' included): ");
    } while(bookings < 0 || bookings > seats_free);             // seats_free < n*m
    
    
    
    if(bookings == 0){
        // sending seats number to server
        if((write(conn_s, "0", sizeof(char))) == -1)                                     // write 4
            error("write 4 failed");
        
        raise(SIGUSR1);
    }
    
    
    
    book_size = (log10(bookings) + 2) * sizeof(char);
    
    if((buff = malloc(seat_size)) == NULL)
        error("memory allocation failed.");
    
    snprintf(buff, book_size, "%ld", bookings);
    
#ifdef DEBUG
    printf("buff: %s\n", buff);
    fflush(stdout);
#endif
    
    
    // sending seats number to server
    if((write(conn_s, buff, book_size)) == -1)                                     // write 4
        error("write 4 failed");
    
    // to avoid duplicates
    if((redundancy = malloc(sizeof(char) * (n * m))) == NULL)
        error("memory allocation failed");
    
    if((msg = malloc(MSG_SIZE * sizeof(char))) == NULL)
        error("memory allocation failed");
    
    // recover seats and sending to server
    // retrieve the answer
    do{
        bzero(redundancy, sizeof(char) * (n * m));
        for(int i=0; i<bookings; i++){
            
            snprintf(msg, MSG_SIZE * sizeof(char), "Enter the %d° seat:", i + 1);
            seat = get_long(msg);
            
            // checking the boundaries
            if(seat < 1 || seat > n*m){
                printf("Please, enter a number between 1 and %d\n", (n*m));
                fflush(stdout);
                i--;
                continue;
            }
            
            // checking the seat avaiability
            if(redundancy[seat - 1] != '\0'){
                puts("Seat already entered\n");
                i--;
                continue;
            }
            
            redundancy[seat - 1] = '1';
            snprintf(buff, log10(seat) + 2, "%d", seat);
    
            // sending the seat to server
            if((write(conn_s, buff, log10(seat) + 2)) == -1)                       // write 5
                error("write 5 failed");
        }
        
        // retrieving the answer from server
        res = read(conn_s, buff, 2 * sizeof(char));                           // read 6
        
        if(res == -1){
            error("read 6 failed.");
        } else if(res == 0)
            raise(SIGUSR1);
        
        buff[1] = '\0';
        
#ifdef DEBUG
        printf("Received from read 6: %d, %s\n", atoi(buff), buff);
        fflush(stdout);
#endif

        if (atoi(buff) != 0) {
            switch(atoi(buff)) {
                case 1:
                    printf("Selected seats are not available together\n");
                    break;
                    
                default:
                    printf("Generic error code %s\n", buff);
                    break;
            }
            
            
            while(strcmp(buff, "y") != 0 && strcmp(buff, "Y") != 0 && strcmp(buff, "n") != 0 && strcmp(buff, "N") != 0){
                printf("Retry? (y/n)\n");
                fflush(stdout);
                
                scanf("%s", buff);
                
#ifdef DEBUG
                printf("buff: %s\n", buff);
                printf("size of buff: %ld\n", strlen(buff));
#endif
                
                // getchar needs to avoid the trailing '\n'
                getchar();
            }
            
            
            // sending to server the decision of user to re-enter seats
            if((write(conn_s, buff, sizeof(char))) == -1)                          // write 7
                error("write 7 failed");
        }
        else printf("Operations succeded.\n");
        
#ifdef DEBUG
        printf("BUFFER: %s\n", buff);
#endif
    
    } while(buff[0] == 'y' || buff[0] == 'Y');
    
    
    communicated = true;
    
    if(buff[0] == 'n' || buff[0] == 'N')
        return false;
    
    return true;
}


void receive3(){
    int res;
    char *code;
    
    if((code = malloc(sizeof(char) * CODE_SIZE)) == NULL)
        error("memory allocation failed.");
    
    res = read(conn_s, code, sizeof(char) * CODE_SIZE);            // read 8
    
    if(res == -1){
        error("read 8 failed.");
    } else if(res == 0)
        raise(SIGUSR1);
    
    
    printf("*************************************************\n");
    printf("*\t YOUR BOOKING CODE IS: %s \t*\n", code);
    printf("*************************************************\n");
    
    // canceling the alarm for ending the booking path
    alarm(0);
}


void check_semaphore_state(){
    char buff;
    int counter = 0;
    
    if((read(conn_s, &buff, sizeof(char))) == -1)            // read semaphore state
        error("read semaphore state failed.");
    
    // endless check about server availability
    fflush(stdout);
    while(buff == '0'){
        printf("|");
        fflush(stdout);
        
        if((read(conn_s, &buff, sizeof(char))) == -1)            // read semaphore state
            error("read semaphore state failed.");
        
        counter++;
    }
    if (counter > 0)
        printf("\nWaiting time: %d sec\n", counter);
}


long get_port(int argc, char *argv[]){
    long port;
    char *endptr;

    
    switch(argc){
        case 1:
            port = DEFAULT_PORT;
            break;
            
        case 3:
        case 5:
            if(strcmp(argv[1], "-p") == 0){
                errno = 0; // reset error number
                port = strtol(argv[2], &endptr, 10);
                
                if(errno == ERANGE ||                       // number is too small or too large
                    endptr == argv[2] ||                    // no character was read
                    (*endptr && *endptr != '\n') ||         // *endptr is neither end of string nor newline, so we didn't convert the *whole* input
                    port < 1024 || port > 65535){           // port must be ephemeral or non-privileged
                    
                    error("USAGE: ./server [-p <PORT_NUMBER>] [-a <SERVER_ADDRESS>], port number must be ephemeral or non-privileged");
                }
            }
            else if(strcmp(argv[1], "-a") == 0)
                port = DEFAULT_PORT;
            else
                error("USAGE: ./server [-p <PORT_NUMBER>] [-a <SERVER_ADDRESS>]");
            break;
            
        default:
            error("USAGE: ./server [-p <PORT_NUMBER>] [-a <SERVER_ADDRESS>]");
            break;
    }
    
    return port;
}


char *get_address(int argc, char *argv[]){
    char *address;
    // localhost ranges from 127.0. 0.0 to 127.255. 255.255
    
    switch(argc){
        case 1:
            address = LOCALHOST;
            break;
            
        case 3:
            if(strcmp(argv[1], "-a") == 0)
                address = check_address_format(argv[2]);
            else
                // in this branch, argv[1] must be "-p" because otherwise
                // get_port() would have recognized the error
                address = LOCALHOST;
            break;
            
        case 5:
            if(strcmp(argv[3], "-a") == 0){
                address = check_address_format(argv[4]);
            }
            else
                error("USAGE: ./server [-p <PORT_NUMBER>] [-a <SERVER_ADDRESS>]");
            break;
            
        default:
            error("USAGE: ./server [-p <PORT_NUMBER>] [-a <SERVER_ADDRESS>]");
            break;
    }
    return address;
}


char *check_address_format(char *address){
    regex_t regex;
    int res;
    
    // compiling the regex: it will accept only strings which represent an IP address
    res = regcomp(&regex, "^(([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])\\.){3}([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])$", REG_EXTENDED);
    if (res != 0)
        error("could not compile regex");
    
    
    // execute regular expression
    res = regexec(&regex, address, 0, NULL, 0);
    
    // free memory allocated to the pattern buffer by regcomp()
    regfree(&regex);
    
    if (res == 0)
        return address;
    else{
#ifdef DEBUG
        char msgbuf[100];
        regerror(res, &regex, msgbuf, sizeof(msgbuf));
        fprintf(stderr, "Regex match failed: %s\n", msgbuf);
#endif
        error("USAGE: ./server [-p <PORT_NUMBER>] [-a <SERVER_ADDRESS>], wrong ip format");
    }
}


bool check_email_format(char *email){
    regex_t regex;
    int res;
    
    // compiling the regex: it will accept only strings which represent an email
    res = regcomp(&regex, "^[A-Za-z0-9_\\-\\.]+@([A-Za-z0-9_-]+\\.)+([A-Za-z0-9_-]){2,4}$", REG_EXTENDED);
    if (res != 0)
        error("could not compile regex");
    
    
    // execute regular expression
    res = regexec(&regex, email, 0, NULL, 0);
    
    // free memory allocated to the pattern buffer by regcomp()
    regfree(&regex);
    
    if (res == 0)
        return true;
    else{
        
#ifdef DEBUG
        char msgbuf[100];
        regerror(res, &regex, msgbuf, sizeof(msgbuf));
        fprintf(stderr, "Regex match failed: %s\n", msgbuf);
#endif
        
        error("wrong email address");
    }
}





