#include "../utils/utils.h"

#define BACKLOG 10
#define SEATS_FILE_NAME "cinema_struct"
#define BOOKING_FILE_NAME "booking_struct"
#define ACCOUNTS_FILE_NAME "accounts"
#define NUM_ERR 1
#define MAX_ACCOUNT_LINE_SIZE 1024

#define BOOKING_CRITICAL_SECTION_INDEX 0
#define SIGNUP_CRITICAL_SECTION_INDEX 1
#define DELETING_CRITICAL_SECTION_INDEX 2




typedef struct thread_arguments{
    char * code;
    int conn_s;
} t_args;


typedef struct reservation{
    char *code;
    struct reservation *next;
} reservation_t;


typedef struct person{
    struct reservation *res_head;
    struct person *next;
    bool in_critical_section;
    char *email;
    char *nickname;
    char *psw;
    int semfd;
} person_t;



// method signatures
void send1();
void receive0();
int user_access();
int get_decision();
void setup_events();
void release_token();
void wait_for_token();
void print_accounts();
char *get_random_code();
int startup_semaphore();
void sync_cinema_file();
void check_config_files();
void sync_accounts_file();
char *create_struct_file();
char *create_booking_file();
void sync_prenotazioni_file();
void startup_seats_file(int fd);
bool delete_booking(char *code);
bool remove_booking(char *code);
person_t *create_accounts_file();
void *child_func(void *arguments);
void get_user_info(int access_type);
char *retrieve_username(char *email);
long get_port(int argc, char *argv[]);
void init_person_list(person_t **head);
person_t *check_mail_exists(char *email);
reservation_t *retrieve_booking (char *code);
bool receive2(int **seats_array, int *bookings);
void startup_connection(int *list_s, long port);
void init_reservation_list(reservation_t **head);
void send3(int *seats_array, int bookings, char *code);
person_t *check_account_exists(char *email, char *password);
void *add_reservation_after(reservation_t *prev, char *code);
void fill_bookings(int *seats_array, int bookings, char *code);
void *add_person_after(person_t *prev, char *email, char *nickname, char *psw, reservation_t *reserv);




// global variables
int n;                            // # rows
int m;                            // # cols
int semfd;                        // semaphore file descriptor
char *cinema;                     // cinema address
char *booking_addr;               // booking array address
pthread_t main_tid;               // main thread id (TID)
person_t *accounts;

// thread local variables
__thread person_t *current_account = NULL;
__thread int conn_s;              // conenction socket
__thread bool connected = false;
__thread bool in_signup_critical_section = false;
__thread bool in_booking_critical_section = false;




#define error(msg) {\
            fprintf(stderr, msg);\
            printf("\nError -> %s\n", strerror(errno));\
            fflush(stdout);\
            if(connected == true) { connected = false; close(conn_s); puts("closing connection error"); }\
            if(in_booking_critical_section == true) { in_booking_critical_section = false; release_token(BOOKING_CRITICAL_SECTION_INDEX); }\
            if(in_signup_critical_section == true) { in_signup_critical_section = false; release_token(SIGNUP_CRITICAL_SECTION_INDEX); }\
            if(current_account->in_critical_section == true) { current_account->in_critical_section = false; release_token(DELETING_CRITICAL_SECTION_INDEX); }\
            exit(EXIT_FAILURE);\
}\


void event_handler(int signal){
    void *status;
    
    printf("\nsignal received: %d\n", signal);
    
    if(main_tid == pthread_self()){
        // saving memory address containing bookings
        sync_prenotazioni_file();
        
        // saving memory address containing seats
        sync_cinema_file();
        
        // saving the structure containing accounts and their reservations
        sync_accounts_file();
    }
    
    
        
    // the last socket created is both in main and one of its children
    // so only one of them
    if(connected == true && main_tid != pthread_self()){
        connected = false;
        if (close(conn_s) < 0)
            puts("server: connection closing failed.");
    }
    
    
    // main does not enter in booking critical section, and only one thread at a 
    // time does it, so the token can be released by only one of them
    if(in_booking_critical_section == true){
        in_booking_critical_section = false;
        release_token(BOOKING_CRITICAL_SECTION_INDEX);
    }
    
    // main does not enter in signup critical section, and only one thread at a 
    // time does it, so the token can be released by only one of them
    if(in_signup_critical_section == true){
        in_signup_critical_section = false;
        release_token(SIGNUP_CRITICAL_SECTION_INDEX);
    }
    
    // main does not enter in deleting critical section, and only one thread at a 
    // time does it, so the token can be released by only one of them
    if(current_account != NULL && current_account->in_critical_section == true){
        current_account->in_critical_section = false;
        release_token(DELETING_CRITICAL_SECTION_INDEX);
    }
    
    
    if(main_tid == pthread_self()){
        munmap((void *) cinema, n * m * sizeof(char));
        munmap((void *) booking_addr, n * m * CODE_SIZE * sizeof(char));
        
        exit(EXIT_FAILURE);
    }
        
    pthread_exit(&status);
}




int main(int argc, char *argv[]){
    int         list_s;                       // listening socket
    long        port;                         // port used for the connection
    struct      sockaddr_in connaddr;         // connection socket address
    socklen_t   socket_in_size;               // size of client address
    
    
    main_tid = pthread_self();
    
    // retreive the port number from cmd line 
    port = get_port(argc, argv);
    
    // handling events
    setup_events();
    
    // check about the compliance of files
    check_config_files();
    
    cinema = create_struct_file();
    booking_addr = create_booking_file();
    accounts = create_accounts_file();
    
#ifdef DEBUG
    for(int i=0; i<n;i++){
        for(int j=0;j<m;j++){
            printf("%c", cinema[i * m + j]);
        }
        puts("");
    }
    
    printf("cinema: %p\n", cinema);
    fflush(stdout);
#endif
    
    // initializing communication components
    startup_connection(&list_s, port);
    
    semfd = startup_semaphore();
    
    // itialization of random num generator
    srand(time(NULL));
    
    // waiting for a connection
    while(1){
        char *code = get_random_code();
        
        socket_in_size = sizeof(struct sockaddr_in);
        
        if ((conn_s = accept(list_s, (struct sockaddr *) &connaddr, &socket_in_size)) < 0)
            error("server: error during the accept\n");
        
        // now the main process is connected
        connected = true;
        
        
    
        struct timeval recv_timeout;      
        recv_timeout.tv_sec = 120; // Number of whole seconds of elapsed time
        recv_timeout.tv_usec = 0; // Number of microseconds of rest of elapsed time minus tv_sec. Always less than one million
        
        if (setsockopt (conn_s, SOL_SOCKET, SO_RCVTIMEO, &recv_timeout, sizeof(recv_timeout)) < 0)
            error("setsockopt failed\n");
        
        
        // set timeout timer for receive and send data
        struct timeval send_timeout;      
        send_timeout.tv_sec = 5; // Number of whole seconds of elapsed time
        send_timeout.tv_usec = 0; // Number of microseconds of rest of elapsed time minus tv_sec. Always less than one million
        
        if (setsockopt (conn_s, SOL_SOCKET, SO_SNDTIMEO, &send_timeout, sizeof(send_timeout)) < 0)
            error("setsockopt failed\n");
        
        
        
        // printing the address of connection socket
        printf("server: connection from %s\n", inet_ntoa(connaddr.sin_addr));
        fflush(stdout);
        
        t_args *arguments = malloc(sizeof(*arguments));
        arguments->code = code;
        arguments->conn_s = conn_s;
        
        
        pthread_t tid;
        pthread_create(&tid, NULL, child_func, (void *) arguments); // code and conn_s
    }
}




void *child_func(void *arguments){
    void *status;
    int bookings;
    int *seats_array;
    int decision;
    int access_type;
    t_args *args = (t_args *) arguments;
    
    conn_s = args->conn_s;
    connected = true;
    
    puts("");
    fflush(stdout);
    
    if((access_type = user_access()) != WANT_TO_EXIT){
        get_user_info(access_type);
    
        if((decision = get_decision()) == WANT_TO_BOOK){
            send1();
            
            if(receive2(&seats_array, &bookings))
                send3(seats_array, bookings, args->code);
            
            free(seats_array);
        } else if(decision == WANT_TO_CANCEL){
            receive0();                
        }
    }
    
    
    puts("server: closed connection");
    fflush(stdout);

    // closing connection socket description
    if (close(args->conn_s) < 0)
        error("server: closing connection failed.");
    
    free(args->code);
    pthread_exit(&status);
}




int startup_semaphore(){
    int fd;
    
    if((fd = semget(IPC_PRIVATE, 2, IPC_CREAT | IPC_EXCL | 0666)) == -1)
        error("server: semaphore creation failed");
    
    if(semctl(fd, 0, SETVAL, 1) == -1)
        error("server: semaphore control operation failed");
    
    if(semctl(fd, 1, SETVAL, 1) == -1)
        error("server: semaphore control operation failed");
    
    return fd;
}




int get_decision(){
    char *buff;
    int res;
    
    if((buff = malloc(sizeof(char))) == NULL)
        error("server: memory allocation failed");
    
    
    // reading the decision from the client
    res = read(conn_s, buff, sizeof(char));                                     // read -1

    if(res == -1){
        error("read -1 failed");
    } else if(res == 0)
        raise(SIGINT);
    
    
    if(*buff == '1'){
        free(buff);
        return WANT_TO_BOOK;
    } else if(*buff == '2') {
        free(buff);
        return WANT_TO_CANCEL;
    } else {
        free(buff);
        return WANT_TO_EXIT;
    }
}




void startup_connection(int *list_s, long port){
    struct sockaddr_in listaddr;                // listening socket address
    
    printf("server: listening from port %ld.\n", port);
    fflush(stdout);
    
    // creating the listening socket 
    if ((*list_s = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        error("server: error creating socket.\n");
    
    
    // resetting serveraddr structure   
    // and fill it with relevant data
    memset(&listaddr, 0, sizeof(listaddr));
    listaddr.sin_family = AF_INET;
    listaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    listaddr.sin_port = htons(port);
    
    
    // to avoid "Address already in use" error
    int optval = 1;
    setsockopt(*list_s, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int));
    
    
    // binding listening socket
    if (bind(*list_s, (struct sockaddr *) &listaddr, sizeof(listaddr)) < 0)
        error("server: error during bind.\n");
        
    
    // make the server listening
    if (listen(*list_s, BACKLOG) < 0)
        error("server: error during listen.\n");
}




void check_config_files(){
    // if one of this files does not exists, then all the existing ones must be deleted
    if(access(SEATS_FILE_NAME, F_OK) || access(BOOKING_FILE_NAME, F_OK) || access(ACCOUNTS_FILE_NAME, F_OK)){
        if(!access(SEATS_FILE_NAME, F_OK))
            unlink(BOOKING_FILE_NAME);
        
    
        if(!access(BOOKING_FILE_NAME, F_OK))
            unlink(BOOKING_FILE_NAME);
        
        
        if(!access(ACCOUNTS_FILE_NAME, F_OK))
            unlink(ACCOUNTS_FILE_NAME);
        
    }
}




char *create_struct_file(){
    int fd;                                     // file descriptor of "cinema_struct" file
    int file_len;                               // the len of file
    char *buff;                                 // temporary buffer
    char *addr;                                 // address to the shared mem. that contains the seats infos
    ssize_t memlen;                             // the length of the shared mem.
    
    
    
    // exclusive creation of file
    if((fd = open(SEATS_FILE_NAME, O_CREAT|O_EXCL|O_RDWR, 0666)) == -1){        
        // retry the file opening without exclusive creation
        if((fd = open(SEATS_FILE_NAME, O_RDWR, 0666)) == -1)
            error("server: seats file opening failed.");
        
    } else {
        // if the just is just created we need
        // to fill it with some information
        startup_seats_file(fd);
    }
    
    
    // retrieving the length of file
    file_len = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);
    
    
    if((buff = malloc(file_len)) == NULL)
        error("server: dynamic memory allocation failed.");
        
    
    // reading the "cinema_struct" file
    if(read(fd, buff, file_len) == -1)
        error("server: error during file reading.\n");
    
    
    // reading file infos
    n = atoi(strtok(buff, ";"));
    m = atoi(strtok(NULL, ";"));
    
    if(!n || !m)
        error("server: integer conversion failed.");
    
    memlen = n * m * sizeof(char);
    
    if((addr = (char *) mmap(NULL, memlen, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0)) == MAP_FAILED)
        error("server: memory mapping failed.");
    
    
#ifdef DEBUG
    printf("%d, %d\n", n, m);
    fflush(stdout);
    printf("addr: %p\n", addr);
    fflush(stdout);
#endif
    
    // copy the file inside the memory pointed by "addr"
    for(int i=0; i<n; i++){
        strncpy(addr, strtok(NULL, ";"), m);
        addr += m * sizeof(char);
    }
    
    free(buff);
    close(fd);
    
    return addr - memlen;
}




char *create_booking_file(){
    int fd;
    int counter = 0;
    char *addr;
    size_t size = CODE_SIZE + 2;
    char *buff; // +2 includes '\n' and '\0'
    FILE *file;
    
    
    if((buff = malloc(sizeof(char) * size)) == NULL)
        error("server: memory allocation failed");
    
    
    // exclusive creation of file
    if((fd = open(BOOKING_FILE_NAME, O_CREAT|O_EXCL|O_RDWR, 0666)) == -1){
        // retry the file opening without exclusive creation
        if((fd = open(BOOKING_FILE_NAME, O_RDWR, 0666)) == -1)
            error("server: seats file opening failed.");
    }
    
    // creating the shared memory mapping
    if((addr = (char *) mmap(NULL, n * m * CODE_SIZE * sizeof(char), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0)) == MAP_FAILED)
        error("server: memory mapping failed.");
    
    
    if ((file = fdopen(fd, "w+")) == NULL)
        error("server: error while conversion from file descriptor to FILE *");
    
    while(getline(&buff, &size, file) > 0){
        if(buff[0] != '\n'){
            memcpy(addr + CODE_SIZE * counter, buff, CODE_SIZE);
        }
        counter++;
    }
    
    
    close(fd);
    fclose(file);
    free(buff);
    
    return addr;
}




void startup_seats_file(int fd){
    long n = 1;                           // # of rows
    long m = 1;                           // # of cols
    char *row;                       // 0's row
    char *initial_infos;             // initial file infos
    
    // controls about the initial infos provided
    do{
        if(n<1 || m<1 || n>100 || m>100){
            printf("Warning, the number of cols and rows have to be between 1 and 100\n");
            fflush(stdout);
        }
        
        n = get_long("\nEnter the number of rows: ");
        
        m = get_long("\nEnter the number of columns: ");
                
    } while (n<1 || m<1 || n>100 || m>100);
    puts("");
    fflush(stdout);
    
    
    // getting initial info's size to write them of file: n;m;\0
    size_t size = floor(log10(n)) + 1 + floor(log10(m)) + 1 + 3;
    
    if((initial_infos = malloc(size)) == NULL)
        error("server: memory allocation failed.");
    
    snprintf(initial_infos, size, "%ld;%ld;", n, m);
    
#ifdef DEBUG
    printf("initial infos: %s\n", initial_infos);
    fflush(stdout);
#endif
    
    
    // writing initial infos on file
    if(write(fd, initial_infos, strlen(initial_infos)) == -1)
        error("server: failed to write first line of the file\n");
    
    
    if((row = malloc(m + 1)) == NULL)
        error("server: memory allocation failed");
    
    // creating a 0's row
    for(int j=0; j<m; j++){
        row[j] = '0';
    }
    row[m] = ';';
    
    // writing seats on file
    for(int i=0; i<n; i++){
        if(write(fd, row, m+1) == -1)
            error("server: failed to write seats line of file\n");
    }
    
    free(row);
    free(initial_infos);
}



void receive0(){
    char *buff;
    
    if((buff = malloc(sizeof(char) * CODE_SIZE)) == NULL)
        error("memory allocation failed");
    
    // reading the code from the client
    if((read(conn_s, buff, CODE_SIZE)) == -1)                                     // read 0.1
        error("read 0.1 failed");
    
#ifdef DEBUG
    printf("read %s\n", buff);
    fflush(stdout);
#endif
    
    if(remove_booking(buff) == true){
        // writing the answer to the server
        if((write(conn_s, "1", sizeof(char))) == -1)                                      // write 0.2
            error("write 0.2 failed");
        
    } else{
        // writing the answer to the server
        if((write(conn_s, "0", sizeof(char))) == -1)                                      // write 0.2
            error("write 0.2 failed");
    }
    
    free(buff);
}




// sending to the client, the seats availability, row by row
void send1(){
    bool cinema_full = true;
    char *tmp;
  
    
    // sends # rows to the client
    if((write(conn_s, (char *) (&n), sizeof(int))) == -1)                                   // write 1
        error("server: write 1 failed");
    
    // sends # cols to the client
    if((write(conn_s, (char *) (&m), sizeof(int))) == -1)                                   // write 2
        error("server: write 2 failed");
    
    // waiting for the round
    wait_for_token(BOOKING_CRITICAL_SECTION_INDEX);
    
    
    // sends a seats row to the client
    for(int i=0; i<n; i++){
        
        tmp = cinema + i * m * sizeof(char);
        
        if(write(conn_s, tmp, m * sizeof(char)) == -1)  // write 3
            error("server: write 3 failed");
        
        for(int j=0; j<m && cinema_full; j++){
            if(tmp[j] == '0')
                cinema_full = false;
        }
    }
    
#ifdef DEBUG
    printf("n: %d; m: %d\n", n, m);
    fflush(stdout);
#endif
    
    if(cinema_full)
        raise(SIGINT);
}




// receiving from client the seats to book
bool receive2(int **seats_array, int *bookings) {
    void *status;
    int res;                                                        // to handle socket message reading
    char *buff;                                                     // generic buffer to store data
    int bookable;                                                   // checks the availability of seats
    ssize_t seat_size = (log10(n * m) + 2) * sizeof(char);          // # max digits for a seat

    if((buff = malloc(seat_size)) == NULL)
        error("server: memory allocation failed.");
    
    
    // receive from client the number of seats to book
    res = read(conn_s, buff, seat_size);                                      // read 4
    
    if(res == -1){
        error("server read 4 failed.");
    } else if(res == 0)
        raise(SIGINT);
    
    
    // if buff is '0', close connection -> client doesn't want booking anymore
    if(buff[0] == '0'){
        release_token(BOOKING_CRITICAL_SECTION_INDEX);
        free(buff);
        close(conn_s);
        pthread_exit(&status);
    }
    
    
    if(((*bookings) = atoi(buff)) == 0)
        error("server: atoi failed.");
    
    if((*seats_array = malloc((*bookings) * sizeof(int))) == NULL)
        error("server: memory allocation faield.");
    
    
#ifdef DEBUG
    printf("bookings: %d\n", *bookings);
    fflush(stdout);
#endif
    
    do {
        // initializing the bookability to "OK" (true)
        bookable = 0;

        // reads all the seats wanted and checks if they are bookable
        for (int i = 0; i < *bookings; i++) {
            
            
            // receiving the seat from client
            res = read(conn_s, buff, seat_size);                        // read 5 
            
            if(res == -1){        
                error("error: read 5 failed.");
            }
            else if(res == 0)
                raise(SIGINT);
            
            if(((*seats_array)[i] = atoi(buff)) == 0)
                error("error: integer conversion failed.");
            
#ifdef DEBUG
            printf("\nread 5: %s\n", buff);
            fflush(stdout);
            printf("seats_array[%d] = %d\n", i, (*seats_array)[i]);
            fflush(stdout);
            printf("memory content %c\n", *(char *)(cinema + ((*seats_array)[i] - 1) * sizeof(char)));
            fflush(stdout);
#endif
            
            if (*(char *)(cinema + ((*seats_array)[i] - 1) * sizeof(char)) != '0') {
                bookable = 1;
#ifdef DEBUG
                printf("joined in round %d\n", i);
                fflush(stdout);
#endif
                continue;
            }
        }
        
        snprintf(buff, 2 * sizeof(char), "%d", bookable);
        
#ifdef DEBUG
        printf("sending from write 6: %d, %s\n", atoi(buff), buff);
        fflush(stdout);
#endif
        
        // sending bookable variable
        if((write(conn_s, buff, 2 * sizeof(char))) == -1)                           // write 6
            error("server: write 6 failed.");

        if(bookable != 0){
            // if seats are not bookable, waiting for the will of retry answer
            res = read(conn_s, buff, 2 * sizeof(char));                             // read 7
            
            if(res == -1){        
                error("server: read 7 failed.");
            }
            else if(res == 0)
                raise(SIGINT);
        }
        
    } while (buff[0] == 'y' || buff[0] == 'Y');

    
    
    
    
    // handling the user answer/the correctness of the input
    if (buff[0] == 'n' || buff[0] == 'N') {
        puts("Input canceled");
    }
    else {
        for (int i = 0; i < *bookings; i++) {
            cinema[(*seats_array)[i] - 1] = '1';
        }
        
        puts("Input gone well");
    }
    
#ifdef DEBUG
    printf("(SEATS) memory area content: ");
    fflush(stdout);
    
    // stampa del contenuto dell'area di memoria
    for(int i = 0; i < n * m; i++){
        printf("%c", cinema[i]);
    }
    puts("");
    fflush(stdout);
#endif
    
    release_token(BOOKING_CRITICAL_SECTION_INDEX);
    
    if(buff[0] == 'n' || buff[0] == 'N'){
        free(buff);
        return false;
    }
    
    free(buff);
    return true;
}




// sending to the client, booking code
void send3(int *seats_array, int bookings, char *code){
    fill_bookings(seats_array, bookings, code);
        
    printf("code sent to the client: %s\n", code);
    fflush(stdout);
    
    // sends random code to the client
    if((write(conn_s, code, CODE_SIZE * sizeof(char))) == -1)                                   // write 8
        error("server: write 8 failed");
    
    // add the reservation code to the current account
    add_reservation_after(current_account->res_head, code);
}




void fill_bookings(int *seats_array, int bookings, char *code){
    for(int i=0; i<bookings; i++){
        strncpy(booking_addr + (seats_array[i] - 1) * CODE_SIZE * sizeof(char), code, CODE_SIZE * sizeof(char));
    }
}




bool remove_booking(char *code){
    bool result = false;
    char *buff;
    
    if((buff = malloc(sizeof(char) * CODE_SIZE)) == NULL)
        error("server: memory allocation failed");
    
    bzero(buff, CODE_SIZE);
    
    wait_for_token(DELETING_CRITICAL_SECTION_INDEX);
    result = delete_booking(code);
    release_token(DELETING_CRITICAL_SECTION_INDEX);
    
    
    if(result == true){
        for(int i = 0; i < n*m; i++){
            memcpy(buff, booking_addr + (i * CODE_SIZE), CODE_SIZE);
            
#ifdef DEBUG
        printf("buff %s, ", buff);
        printf("code %s\n", code);
#endif
            
            if(strcmp(buff, code) == 0){
                memset(booking_addr + (i * CODE_SIZE), 0, CODE_SIZE);
                
                cinema[i] = '0';
            }
        }
    }
    
    return result;
}




void wait_for_token(int sem_index){
    struct sembuf op;
    op.sem_op = -1;
    
    switch(sem_index){
        case BOOKING_CRITICAL_SECTION_INDEX:        
            // instantiating sem op structure
            op.sem_num = sem_index;
            op.sem_flg = IPC_NOWAIT;
    
            while(semop(semfd, &op, 1) == -1){
                if(errno != EAGAIN) // on semop failure with IPC_NOWAIT flag
                    error("server: semaphore operation failed.");
                
                // sends the semaphore state to the client
                if((write(conn_s, "0", sizeof(char))) == -1)                                    // write semaphore state (0)
                    error("server: write semaphore state (0) failed");
                
                sleep(1);
            }
        
            // sends the semaphore state to the client
            if((write(conn_s, "1", sizeof(char))) == -1)                                        // write semaphore state (1)
                error("server: write semaphore state (1) failed");
            
            // to better handle signals
            in_booking_critical_section = true;
            break;
            
            
        case SIGNUP_CRITICAL_SECTION_INDEX:        
            // instantiating sem op structure
            op.sem_num = sem_index;
            op.sem_flg = 0;
            
            if(semop(semfd, &op, 1) == -1)
                error("server: semaphore operation failed.");
            
            // to better handle signals
            in_signup_critical_section = true;
            break;
            
            
        case DELETING_CRITICAL_SECTION_INDEX: 
            // instantiating sem op structure
            op.sem_num = 0;
            op.sem_flg = 0;
            
            if(semop(current_account->semfd, &op, 1) == -1)
                error("server: semaphore operation failed.");
            current_account->in_critical_section = true;
            break;
            
        default:
            error("server: wrong semaphore index.");
    }
}




void release_token(int sem_index){
    // instantiating sem op structure
    struct sembuf op;
    op.sem_op = 1;
    op.sem_flg = 0;
    
    if(sem_index == DELETING_CRITICAL_SECTION_INDEX){
        op.sem_num = 0;
        
        if(semop(current_account->semfd, &op, 1) == -1)
            error("server: semaphore operation failed.");
        
        current_account->in_critical_section = false;
    } else {
        op.sem_num = sem_index;
        
        if(semop(semfd, &op, 1) == -1)
            error("server: semaphore operation failed.");
        
        // to better handle signals
        if(sem_index == BOOKING_CRITICAL_SECTION_INDEX)
            in_booking_critical_section = true;
        else
            in_signup_critical_section = true;
    }
    
}




char * get_random_code(){
    bool ok;                           // flag for handle random code choice
    int code;
    char *str_code;
    char *buff;
    
    if((str_code = malloc((CODE_SIZE + 1) * sizeof(char))) == NULL)
        error("server: memory allocation failed")
    
    if((buff = malloc((CODE_SIZE + 1) * sizeof(char))) == NULL)
        error("server: memory allocation failed")
        
    // generating random codes without duplicates
    do{
        ok = true;
        
        code = ((int) rand() % (RAND_MAX - 1000000000) + 1000000000); // 1 MLD
        snprintf(str_code, (CODE_SIZE + 1) * sizeof(char), "%d", code);
        
        
        for(int i=0; i<n*m; i++){
            memcpy(buff, booking_addr + i * CODE_SIZE * sizeof(char), CODE_SIZE * sizeof(char));
            
            if(strcmp(buff, str_code) == 0){
                ok = false;
                break;
            }
        }
    }while(!ok);
    
    free(buff);
    
    return str_code;
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
    
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGHUP, &act, NULL);
    sigaction(SIGTERM, &act, NULL);
    sigaction(SIGQUIT, &act, NULL);
    // because there is a connection acceptance in main process
    sigaction(SIGPIPE, &act, NULL);
    // because illegal instructions come from outsiders
    sigaction(SIGILL, &act, NULL);
    // because segmentation errors can be forced bringing undefined behaviours
    sigaction(SIGSEGV, &act, NULL);
    
    
    // because is not a problem form the main process if a child of its dies
    // while children can't receive SIGCHLD from system, but only from the 
    // outside, because none of them fork furthermore
    signal(SIGCHLD, SIG_IGN);
    // because we don't define a meaning for both of them
    signal(SIGUSR1, SIG_IGN);
    signal(SIGUSR2, SIG_IGN);
}



// stores the state of the cinema in the file "cinema_struct"
void sync_cinema_file(){
    int     fd;
    size_t  size;
    char    *initial_infos;


    if((fd = open(SEATS_FILE_NAME, O_WRONLY|O_TRUNC, 0666)) == -1)                     // file opened is trunced because is going to be rewritten
        error("server: seats file opening failed.");

    size = sizeof(char) * (floor(log10(n)) + 1 + floor(log10(m)) + 1 + 3);             // copied from create struct file, the ending + 1 is for the \n
    if((initial_infos = malloc(size)) == NULL)
        error("server: memory allocation failed.");

    snprintf(initial_infos, size, "%d;%d;", n, m);                                     // sizeof(char) is for the \n
    write(fd, initial_infos, size - sizeof(char));


    for(int i = 0; i < n; i++){                                    // cycling rows
        for(int j = 0; j < m; j ++){                               // cycling cols
            if(cinema[m * i + j] == '1')
                write(fd, "1", sizeof(char));
            else if(cinema[m * i + j] == '0')
                write(fd, "0", sizeof(char));
            else
                error("cinema struct memory corrupted");
        }
        write(fd, ";", sizeof(char));
    }

    close(fd);
    
    free(initial_infos);
}



// stores the bookings done until that moment in the file "booking_struct"
void sync_prenotazioni_file(){
    int fd;
    char arr[10];
    
    if((fd = open(BOOKING_FILE_NAME, O_WRONLY|O_TRUNC, 0666)) == -1)     // file opened is truncated because is going to be rewritten
        error("server: seats file opening failed.");
    
    
    for(int i = 0; i < n * m; i++){
        memcpy(arr, &booking_addr[i * CODE_SIZE], sizeof(char) * CODE_SIZE);
        
#ifdef DEBUG
        printf("%10s\n", arr);
        fflush(stdout);
#endif
        
        if(arr[0] != '\0'){
            if(write(fd, arr, sizeof(char) * CODE_SIZE) < 0)
                error("server: error while writing on file");
        }
        
        if(write(fd, "\n", sizeof(char)) < 0)
            error("server: error while writing on file");
    }
    
    close(fd);
}



void sync_accounts_file(){
    int fd;
    char *buff;
    size_t main_info_size;
    reservation_t *curr_res;
    char semi_colons;
    char new_line;
    
    
    semi_colons = ';';
    new_line = '\n';
    
    if((fd = open(ACCOUNTS_FILE_NAME, O_WRONLY|O_TRUNC, 0666)) == -1)                     // file opened is trunced because is going to be rewritten
        error("server: seats file opening failed.");
    
    if((buff = malloc(MAX_ACCOUNT_LINE_SIZE * sizeof(char))) == NULL)
        error("server: memory allocation failed");
    
    accounts = accounts->next;
    
    while(accounts != NULL){
        main_info_size = (strlen(accounts->nickname) + strlen(accounts->email) + strlen(accounts->psw) + 3) * sizeof(char);
        
        snprintf(buff, main_info_size, "%s;%s;%s", accounts->nickname, accounts->email, accounts->psw);
        
        curr_res = accounts->res_head->next;
        
        while(curr_res){
            strncat(buff, &semi_colons, sizeof(char));
            strncat(buff, curr_res->code, CODE_SIZE * sizeof(char));
            
            main_info_size += (CODE_SIZE + 1) * sizeof(char);
            
            curr_res = curr_res->next;
        }
        
        // because our file is formatted in a way that the
        // last line doesn't contain '\n'
        if(accounts->next != NULL){
            strncat(buff, &new_line, sizeof(char));
            
            write(fd, buff, main_info_size);
        } else
            write(fd, buff, main_info_size - sizeof(char));
        
        accounts = accounts->next;
    }
    
    free(buff);
}



long get_port(int argc, char *argv[]){
    long port;
    char *endptr;
    
    switch(argc){
        case 1:
            port = DEFAULT_PORT;
            break;
            
        case 3:
            if(strcmp(argv[1], "-p") == 0){
                errno = 0; // reset error number
                port = strtol(argv[2], &endptr, 10);
                
                if(errno == ERANGE ||                       // number is too small or too large
                    endptr == argv[2] ||                    // no character was read
                    (*endptr && *endptr != '\n') ||         // *endptr is neither end of string nor newline, so we didn't convert the *whole* input
                    port < 1024 || port > 65535){           // port must be ephemeral or non-privileged
                    
                    error("USAGE: ./server [-p <PORT_NUMBER>], port number must be ephemeral or non-privileged");
                }
            }
            else error("USAGE: ./server [-p <PORT_NUMBER>]");
            break;
            
        default:
            error("USAGE: ./server [-p <PORT_NUMBER>]");
            break;
    }
    
    return port;
}



void *add_person_after(person_t *prev, char *nickname, char *email, char *psw, reservation_t *reserv) {
    person_t *node = malloc(sizeof(*node));

    if(!node)
        return NULL;

    node->email = email;
    node->nickname = nickname;
    node->psw = psw;
    node->in_critical_section = false;
    node->res_head = reserv;
    
    
    if((node->semfd = semget(IPC_PRIVATE, 1, IPC_CREAT | IPC_EXCL | 0666)) == -1)
        error("server: semaphore creation failed");
    
    if(semctl(node->semfd, 0, SETVAL, 1) == -1)
        error("server: semaphore control operation failed");
    
    
    
    node->next = prev->next;
    prev->next = node;

    return node;
}



void *add_reservation_after(reservation_t *prev, char *code){
    reservation_t *node = malloc(sizeof(*node));
    
    if(!node)
        return NULL;

    if((node->code = malloc(sizeof(char) * CODE_SIZE)) == NULL)
        error("server: memory allocation failed");
    
    memcpy(node->code, code, CODE_SIZE * sizeof(char));
    node->next = prev->next;
    prev->next = node;

    return node;
}



void init_person_list(person_t **head) {
    *head = malloc(sizeof(**head));
    
    (*head)->email = "admin";
    (*head)->nickname = "admin";
    (*head)->psw = "admin";
    
    (*head)->res_head = NULL;
    (*head)->next = NULL;
}



void init_reservation_list(reservation_t **head) {
    *head = malloc(sizeof(**head));
    (*head)->code = "admin";
    (*head)->next = NULL;
}



void get_user_info(int access_type){
    char *username;
    char *email;
    char *password;
    int res;
    bool ok = false;
    reservation_t *reservation;
    
    if((username = malloc(MAX_INPUT_SIZE * sizeof(char))) == NULL)
        error("server: memory allocation failed");
    
    if((email = malloc(MAX_INPUT_SIZE * sizeof(char))) == NULL)
        error("server: memory allocation failed");
    
    if((password = malloc(MAX_INPUT_SIZE * sizeof(char))) == NULL)
        error("server: memory allocation failed");
    
    
    
    do{
#ifdef DEBUG
        puts("EMAIL");
#endif
        // EMAIL
        res = read(conn_s, email, MAX_INPUT_SIZE * sizeof(char));                                     // read -2.1.1

        if(res == -1){
            error("read -2.1.1 failed");
        } else if(res == 0)
            raise(SIGINT);
        
        
        
        if(access_type == WANT_TO_SIGN_UP){
#ifdef DEBUG
            puts("USERNAME");
#endif
            // USERNAME
            res = read(conn_s, username, MAX_INPUT_SIZE * sizeof(char));                                     // read -2.1.2

            if(res == -1){
                error("read -2.1.2 failed");
            } else if(res == 0)
                raise(SIGINT);
        } else if(access_type == WANT_TO_SIGN_IN){
            username = retrieve_username(email);
        }
        
        
        
        // PASSWORD
#ifdef DEBUG
        puts("PASSWORD");
#endif
        res = read(conn_s, password, MAX_INPUT_SIZE * sizeof(char));                                     // read -2.1.3

        
        if(res == -1){
            error("read -2.1.3 failed");
        } else if(res == 0)
            raise(SIGINT);
        
#ifdef DEBUG
        printf("access type = WANT_TO_SIGN_UP ? %s\n", access_type == WANT_TO_SIGN_UP ? "SI" : "NO");
        printf("read -2.1.1 = %s\n", email);
        printf("read -2.1.2 = %s\n", username);
        printf("read -2.1.3 = %s\n", password);
#endif
        
        
        
        
        if(access_type == WANT_TO_SIGN_UP){
//             wait_for_token(SIGNUP_CRITICAL_SECTION_INDEX);
            if(check_mail_exists(email) == NULL){
                ok = true;
                init_reservation_list(&reservation);
                
                
                
                current_account = add_person_after(accounts, username, email, password, reservation);
                print_accounts();
                
            }
//             release_token(SIGNUP_CRITICAL_SECTION_INDEX);
        } else if(access_type == WANT_TO_SIGN_IN){
            if((current_account = check_account_exists(email, password)) != NULL)
                ok = true;
        }
        
        
        // sending the result to the client
        if((write(conn_s, ok == true ? "1" : "0", sizeof(char))) == -1)                                                // write -2.2
            error("write -2.2 failed");
        
        
        if(access_type == WANT_TO_SIGN_IN && ok == true){
            if((write(conn_s, username, MAX_INPUT_SIZE * sizeof(char))) == -1)                                                // write -2.3
                error("write -2.3 failed");
        }
        
    } while(!ok);
}



person_t *check_mail_exists(char *email){
    person_t *curr = accounts;
    
    while(curr != NULL){
        if(strcmp(curr->email, email) == 0)
            return curr;
        curr = curr->next;
    }
    
    return NULL;
}



person_t *check_account_exists(char *email, char *password){
    person_t *person;
    
    if((person = check_mail_exists(email)) == NULL)
        return NULL;
    
    if(strcmp(person->psw, password) == 0)
        return person;
    
    return NULL;
}



char *retrieve_username(char *email){
    person_t *person;
    
    if((person = check_mail_exists(email)) == NULL)
        return NULL;
    
    return person->nickname;
}



int user_access(){
    char *buff;
    int res;
    
    if((buff = malloc(sizeof(char))) == NULL)
        error("server: memory allocation failed");
    
    
    // reading the decision from the client
    res = read(conn_s, buff, sizeof(char));                                     // read -2

    
    if(res == -1){
        error("read -2 failed");
    } else if(res == 0)
        raise(SIGINT);
    
    
    if(*buff == '1'){
        free(buff);
        return WANT_TO_SIGN_IN;
    } else if(*buff == '2') {
        free(buff);
        return WANT_TO_SIGN_UP;
    } else {
        free(buff);
        return WANT_TO_EXIT;
    }
}



person_t *create_accounts_file(){
    int fd;
    size_t size = MAX_ACCOUNT_LINE_SIZE;
    char *buff;
    FILE *file;
    
    person_t *list = NULL, *p_curr = NULL;
    
    init_person_list(&list);    
    
    if((buff = malloc(sizeof(char) * size)) == NULL)
        error("server: memory allocation failed");
    
    // exclusive creation of file
    if((fd = open(ACCOUNTS_FILE_NAME, O_CREAT|O_EXCL|O_RDWR, 0666)) == -1){
        // retry the file opening without exclusive creation
        if((fd = open(ACCOUNTS_FILE_NAME, O_RDWR, 0666)) == -1)
            error("server: seats file opening failed.");
    }
    
    
    if ((file = fdopen(fd, "w+")) == NULL)
        error("server: error while conversion from file descriptor to FILE *");
    
    while(getline(&buff, &size, file) > 0){
        
        reservation_t *sub_list=NULL, *r_curr=NULL;
        init_reservation_list(&sub_list);
        
        
        char *nickname, *email, *psw;
        
        if((nickname = malloc(MAX_INPUT_SIZE)) == NULL || (email = malloc(MAX_INPUT_SIZE)) == NULL || (psw = malloc(MAX_INPUT_SIZE)) == NULL)
            error("server: memory allocation failed.");
        
        memcpy(nickname, strtok(buff, ";"), MAX_INPUT_SIZE);
        memcpy(email, strtok(NULL, ";"), MAX_INPUT_SIZE);
        memcpy(psw, strtok(NULL, ";"), MAX_INPUT_SIZE);
        
        while(true){
        
            char *code;
            if((code = malloc(CODE_SIZE)) == NULL)
                error("server: memory allocation failed.");
                
            char *a = strtok(NULL, ";");
            if(a == NULL)
                break;
            
            memcpy(code, a, CODE_SIZE);
            
            if(r_curr == NULL)
                r_curr = add_reservation_after(sub_list, code);
            else
                r_curr = add_reservation_after(r_curr, code);
        }
        
        
        if(!p_curr)
            p_curr = add_person_after(list, nickname, email, psw, sub_list);
        else
            p_curr = add_person_after(p_curr, nickname, email, psw, sub_list);
    }
    
    close(fd);
    fclose(file);
    free(buff);
    
    return list;
}
       


void print_accounts(){
    person_t *list = accounts;
    reservation_t *tmp;
    
    
    list = list->next;
    while(list != NULL){
        
        printf("persona %s\n", list->nickname);
        
        puts("reservations");
        
        tmp = list->res_head;
        while(tmp != NULL){
            printf(" %s ", tmp->code);
            tmp = tmp->next;
        }
        
        puts("");
        
        list = list->next;
    }
}



reservation_t *retrieve_booking(char *code){
    reservation_t *curr;
    
    curr = current_account->res_head;
    
    while(curr){
        if(strcmp(curr->code, code) == 0)
            return curr;
        curr = curr->next;
    }
    
    return NULL;
}



bool delete_booking(char *code){
    reservation_t *curr, *deleting;
    
    deleting = retrieve_booking(code);
    
    if(deleting == NULL)
        return false;
        
    curr = current_account->res_head;
    
    while(strcmp(curr->next->code, code) != 0){
        curr = curr->next;
    }
    
    curr->next = deleting->next;
    free(deleting);
    
    return true;
}

























