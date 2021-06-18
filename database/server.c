#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
 
#define DATA_BUFFER 300
 
int currFD = -1;
int currID = -1;
const int SIZE_BUFFER = sizeof(char) * DATA_BUFFER;
 
const char *currDir = "/home/thomasfelix/FINALPROJECT/database/databases";
const char *USERS_TABLE = "./databases/users.csv";
const char *DBLIST_TABLE = "./databases/listDB.csv";
const char *PERMISSION_TABLE = "./databases/listPermission.csv";
 
int create_tcp_server_socket();
int *daemonUtil(pid_t *pid, pid_t *sid);
 
void *menuRoute(void *argv);
void createDatabase(int fd, char *databaseName);
void grantPermission(int fd, char *databaseName, char *username);
void fileLog(char *username, char *string);
bool login(int fd, char *username, char *password);
void registration(int fd, char *username, char *password);
 
int getUserID(const char *path, char *username, char *password);
int getLastID(const char *path);
 
int main() {
    // Please uncomment in last commit
    pid_t pid, sid;
    int *status = daemonUtil(&pid, &sid);
    
    socklen_t addrlen;
    struct sockaddr_in new_addr;
    pthread_t tid;
    char buff[DATA_BUFFER];
    int server_fd = create_tcp_server_socket();
    int new_fd;
 
    while (1) {
        new_fd = accept(server_fd, (struct sockaddr *)&new_addr, &addrlen);
        
        if (new_fd >= 0) {
            printf("Accepted a new connection with fd: %d\n", new_fd);
            pthread_create(&tid, NULL, &menuRoute, (void *) &new_fd);
        }
        
        else {
            fprintf(stderr, "Accept failed [%s]\n", strerror(errno));
        }
    }
    
    return 0;
}
 
void *menuRoute(void *argv) {
    int fd = *(int *) argv;
    char query[DATA_BUFFER], buff[DATA_BUFFER], usernameLogin[100];
    bool log_in = false;
 
    while (read(fd, query, DATA_BUFFER) != 0) {
        puts(query);
 
        if (log_in && query[0] != '\0') {
            char *cmd = strtok(query, ";");
            fileLog(usernameLogin, cmd);
        }

        strcpy(buff, query);
        char *cmd = strtok(buff, " ");

        if (strcmp(cmd, "LOGIN") == 0) {
            char *username = strtok(NULL, " ");
            char *password = (strcmp(username, "root") != 0) ? strtok(NULL, " ") : "root";
            
            if (login(fd, username, password)) {
                strcpy(usernameLogin, username);
                log_in = true;
            }
            
            else {
                break;
            }
        }
        
        else if (strcmp(cmd, "CREATE") == 0) {
            cmd = strtok(NULL, " ");

            if (strcmp(cmd, "USER") == 0) {
                if (currID == 0) {
                    char *username = strtok(NULL, " ");
                    char *password = NULL;
                    cmd = strtok(NULL, " ");

                    if (strcmp(cmd, "IDENTIFIED") == 0) {
                        cmd = strtok(NULL, " ");

                        if (strcmp(cmd, "BY") == 0) {
                            password = strtok(NULL, " ;");
                        }
                    }

                    if (password != NULL) {
                        registration(fd, username, password);
                    }

                    else {
                        write(fd, "Unidentified command!\nShould be CREATE USER [nama_user] IDENTIFIED BY [password_user];\nOr username / password must not have whitespace.\n\n", SIZE_BUFFER);
                    }
                }
                
                else {
                    write(fd, "Forbidden action for command\n\n", SIZE_BUFFER);
                }
            }

            else if (strcmp(cmd, "DATABASE") == 0) {
                if (log_in && currID == 0) {
                    char *databaseName = strtok(NULL, " ;");

                    createDatabase(fd, databaseName);
                }

                else if (log_in && currID > 0) {
                    char *databaseName = strtok(NULL, " ;");

                    createDatabase(fd, databaseName);
                    grantPermission(fd, databaseName, usernameLogin);
                }

                else {
                    write(fd, "Forbidden action for command\n\n", SIZE_BUFFER);
                }
            }
            
            else {
                write(fd, "Invalid CREATE command\n\n", SIZE_BUFFER);
            }
        }

        else if (strcmp(cmd, "GRANT") == 0) {
            cmd = strtok(NULL, " ");

            if (strcmp(cmd, "PERMISSION") == 0) {
                char *databaseName = strtok(NULL, " ");
                char *username = NULL;
                cmd = strtok(NULL, " ");

                if (strcmp(cmd, "INTO") == 0) {
                    username = strtok(NULL, " ;");
                }

                if (username != NULL) {
                    grantPermission(fd, databaseName, username);
                }

                else {
                    write(fd, "Unidentified command!\nShould be GRANT PERMISSION [nama_database] INTO [nama_user];\nOr database / username must not have whitespace.\n\n", SIZE_BUFFER);
                }
            }
        }

        else {
            write(fd, "Invalid query\n\n", SIZE_BUFFER);
        }
    }

    if (fd == currFD) {
        currFD = currID = -1;
    }

    printf("Close connection with fd: %d\n", fd);
    close(fd);
}
 
/****   Controller   *****/
void createDatabase(int fd, char *databaseName) {
    FILE *fileout = fopen(DBLIST_TABLE, "a+");

    int id_user = getLastID(DBLIST_TABLE) + 1;

    if (id_user == 1) {
        fprintf(fileout, "id,database\n");
        fprintf(fileout, "%d,%s\n", id_user, databaseName);
        
        chdir(currDir);
        mkdir(databaseName, 0777);
        chdir("/home/thomasfelix/FINALPROJECT/database/");
        write(fd, "Database successfully added\n\n", SIZE_BUFFER);
    }

    else {
        fprintf(fileout, "%d,%s\n", id_user, databaseName);
        chdir(currDir);

        if (mkdir(databaseName, 0777) == 0) {
            write(fd, "Database successfully added\n\n", SIZE_BUFFER);
        }

        else {
            write(fd, "Database already exists\n\n", SIZE_BUFFER);
        }

        chdir("/home/thomasfelix/FINALPROJECT/database/");
    }

    fclose(fileout);
}

void grantPermission(int fd, char *databaseName, char *username) {
    FILE *fileout = fopen(PERMISSION_TABLE, "a+");

    int id_user = getUserID(PERMISSION_TABLE, username, databaseName);

    if (id_user != -1) {
        write(fd, "Error! Permission has already exists\n\n", SIZE_BUFFER);
    }

    else {
        id_user = getLastID(PERMISSION_TABLE) + 1;

        if (id_user == 1) {
            fprintf(fileout, "id,username,database\n");
            fprintf(fileout, "%d,%s,%s\n", id_user, username, databaseName);
            write(fd, "Permission granted\n\n", SIZE_BUFFER);
        }

        else {
            fprintf(fileout, "%d,%s,%s\n", id_user, username, databaseName);
            write(fd, "Permission granted\n\n", SIZE_BUFFER);
        }
    }

    fclose(fileout);
}

void registration(int fd, char *username, char *password) {
    FILE *fileout = fopen(USERS_TABLE, "a+");

    int id_user = getUserID(USERS_TABLE, username, password);

    if (id_user != -1) {
        write(fd, "Error! User is already registered\n\n", SIZE_BUFFER);
    }
    
    else {
        id_user = getLastID(USERS_TABLE) + 1;
        
        if (id_user == 1) {
            fprintf(fileout, "id,username,password\n");
            fprintf(fileout, "%d,%s,%s\n", id_user, username, password);
            write(fd, "Register success\n\n", SIZE_BUFFER);
        }
        
        else {
            fprintf(fileout, "%d,%s,%s\n", id_user, username, password);
            write(fd, "Register success\n\n", SIZE_BUFFER);
        }
    }

    fclose(fileout);
}
 
bool login(int fd, char *username, char *password) {
    if (currFD != -1) {
        write(fd, "Server is busy, wait for another user to logout.\n", SIZE_BUFFER);
        return false;
    }
 
    int id_user = -1;
    
    if (strcmp(username, "root") == 0) {
        id_user = 0;
    }
    
    else {
        FILE *fileout = fopen(USERS_TABLE, "r");
        if (fileout != NULL) {
            id_user = getUserID(USERS_TABLE, username, password);
        }
        
        fclose(fileout);
    }
 
    if (id_user == -1) {
        write(fd, "Wrong ID or Password!\n", SIZE_BUFFER);
        return false;
    }
    
    else {
        write(fd, "LOGIN success\n", SIZE_BUFFER);
        currFD = fd;
        currID = id_user;
    }

    return true;
}

void fileLog(char *username, char *string) {
    FILE *fileout = fopen("logging.log", "a+");
    time_t t;
    struct tm *temp;
    char timeBuffer[100];

    time(&t);
    temp = localtime(&t);

    strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S", temp);

    fprintf(fileout, "%s:%s:%s", timeBuffer, username, string);

    fprintf(fileout, "\n");

    fclose(fileout);
}

/*****  HELPER  *****/
void getUserDB(char *username2, char *db, char *string, char *string2) {
    int id_user = -1;

    FILE *fileout = fopen(PERMISSION_TABLE, "r");
    
    if (fileout != NULL) {
        char db[DATA_BUFFER], tempdb[DATA_BUFFER];
        bool dbmatch = false;
        int i = 0;
 
        while (fscanf(fileout, "%s", db) != EOF) {
            if (strcmp(tempdb, db) == 0) {
                dbmatch = true;
                break;
            }

            i++;
        }

        if (!dbmatch) {
            strcpy(db, "No database found");
        }

        fclose(fileout);
    }

    FILE *fileout2 = fopen(USERS_TABLE, "r");

    if (fileout2 != NULL) {
        char username2[DATA_BUFFER], tempuser[DATA_BUFFER];
        bool usermatch = false;
        int i = 0;
 
        while (fscanf(fileout2, "%s", username2) != EOF) {
            if (strcmp(tempuser, username2) == 0) {
                usermatch = true;
                break;
            }

            i++;
        }

        if (!usermatch) {
            strcpy(username2, "No user found");
        }

        fclose(fileout2);
    }
}

int getUserID(const char *path, char *string, char *string2) {
    int id_user = -1;
    
    FILE *fileout = fopen(path, "r");
 
    if (fileout != NULL) {
        char db[DATA_BUFFER], inputan[DATA_BUFFER];
        sprintf(inputan, "%s,%s", string, string2);
 
        while (fscanf(fileout, "%s", db) != EOF) {
            char *temp = strstr(db, ",") + 1; // Get username & password from db
            
            if (strcmp(temp, inputan) == 0) {
                id_user = atoi(strtok(db, ","));  // Get id_user from db
                break;
            }
        }

        fclose(fileout);
    }

    return id_user;
}
 
int getLastID(const char *path) {
    int id_user = 0;
    FILE *fileout = fopen(path, "r");
 
    if (fileout != NULL) {
        char db[DATA_BUFFER];
        
        while (fscanf(fileout, "%s", db) != EOF) {
            id_user = atoi(strtok(db, ","));  // Get id_user from db
        }
    }

    return id_user;
}
 
/****   SOCKET SETUP    *****/
int create_tcp_server_socket() {
    struct sockaddr_in saddr;
    int fd, ret_val;
    int opt = 1;
 
    /* Step1: create a TCP socket */
    fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    
    if (fd == -1) {
        fprintf(stderr, "socket failed [%s]\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    printf("Created a socket with fd: %d\n", fd);
 
    /* Initialize the socket address structure */
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(7000);
    saddr.sin_addr.s_addr = INADDR_ANY;
 
    /* Step2: bind the socket to port 7000 on the local host */
    ret_val = bind(fd, (struct sockaddr *)&saddr, sizeof(struct sockaddr_in));
    
    if (ret_val != 0) {
        fprintf(stderr, "bind failed [%s]\n", strerror(errno));
        close(fd);
        exit(EXIT_FAILURE);
    }
 
    /* Step3: listen for incoming connections */
    ret_val = listen(fd, 5);
    if (ret_val != 0) {
        fprintf(stderr, "listen failed [%s]\n", strerror(errno));
        close(fd);
        exit(EXIT_FAILURE);
    }

    return fd;
}
 
int *daemonUtil(pid_t *pid, pid_t *sid) {
    *pid = fork();
 
    if (*pid != 0) {
        exit(EXIT_FAILURE);
    }
    
    if (*pid > 0) {
        exit(EXIT_SUCCESS);
    }
    umask(0);
 
    *sid = setsid();

    if (*sid < 0) {
        exit(EXIT_FAILURE);
    }

    if (chdir(currDir) < 0) {
        exit(EXIT_FAILURE);
    }
 
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
}
