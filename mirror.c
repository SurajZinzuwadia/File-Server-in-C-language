#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>



#define PORT 9001
#define CLIENT_PORT 8080
#define MAX_PATH 1024
#define BUFSIZE 1024
#define BUFFER_SIZE 1024

#define MAX_PATH_LENGTH 1024
#define MAX_TOKENS 128
#define TOKEN_DELIMITER " \t\r\n\a"

char **tokenize(char *line) {
    int bufsize = MAX_TOKENS, position = 0;
    char **tokens = malloc(bufsize * sizeof(char*));
    char *token;

    if (!tokens) {
        fprintf(stderr, "Tokenize: allocation error\n");
        exit(EXIT_FAILURE);
    }

    token = strtok(line, TOKEN_DELIMITER);
    while (token != NULL) {
        tokens[position] = token;
        position++;

        if (position >= bufsize) {
            bufsize += MAX_TOKENS;
            tokens = realloc(tokens, bufsize * sizeof(char*));
            if (!tokens) {
                fprintf(stderr, "Tokenize: allocation error\n");
                exit(EXIT_FAILURE);
            }
        }

        token = strtok(NULL, TOKEN_DELIMITER);
    }
    tokens[position] = NULL;
    return tokens;
}


// Recursive function to search for a file in the directory tree rooted at the given path
int find_file_recursive(const char *root_path, const char *file_name, char *file_info) {
    DIR *dir = opendir(root_path);
    if (dir == NULL) {
        perror("opendir failed");
        return 0;
    }
    struct dirent *entry;
    struct stat statbuf;
    while ((entry = readdir(dir)) != NULL) {
        char entry_path[MAX_PATH_LENGTH];
        snprintf(entry_path, sizeof(entry_path), "%s/%s", root_path, entry->d_name);
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        if (lstat(entry_path, &statbuf) < 0) {
            perror("lstat failed");
            continue;
        }
        if (S_ISDIR(statbuf.st_mode)) {
            // Entry is a directory, search it recursively
            if (find_file_recursive(entry_path, file_name, file_info)) {
                closedir(dir);
                return 1;
            }
        } else {
            // Entry is a file, check if it matches the desired file name
            if (strcmp(entry->d_name, file_name) == 0) {
                // printf("\n*********Stat Buf: %s\n", ctime(&statbuf.st_ctime));
                snprintf(file_info, MAX_PATH_LENGTH, "\nFile name: %s File found: %s Size: %ld Date Created:%s", file_name, entry_path,statbuf.st_size, ctime(&statbuf.st_ctime));
                closedir(dir);
                return 1;
            }
        }
    }
    closedir(dir);
    return 0;
}

// Wrapper function to search for a file in the directory tree rooted at the given path
int find_file(const char *root_path, const char *file_name, char *file_info) {
    char real_path[MAX_PATH_LENGTH];
    printf("root path: %s\n file_name:%s\n", root_path, file_name);
    if (realpath(root_path, real_path) == NULL) {
        perror("realpath failed");
        return 0;
    }
    return find_file_recursive(real_path, file_name, file_info);
}


void handleFileTar(char* findCommand)
{
     // Create temporary file to store file paths
    FILE *fp = fopen("temp.txt", "w");
    if (fp == NULL) {
        perror("Failed to create temporary file");
        exit(1);
    }
    int a;
    a = dup(STDOUT_FILENO);

    if (dup2(fileno(fp), STDOUT_FILENO) == -1) {
        perror("Failed to redirect output");
        exit(1);
    }
    FILE *fp1 = popen(findCommand, "r");
    if (fp1 == NULL) {
        perror("Failed to execute command");
    }
    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), fp1) != NULL) {
        // Do something with each file path, e.g. print it
        printf("%s", buffer);
    }

    pclose(fp1);
    dup2(a, STDOUT_FILENO);
    fclose(fp);
    // Create archive of files in temporary file
    int pid = fork();
    if (pid == -1) {
        perror("Failed to fork");
        exit(1);
    } else if (pid == 0) {
        // Child process
        execlp("tar", "tar", "-czf", "temp.tar.gz", "-T", "temp.txt", NULL);
        perror("Failed to execute tar");
        exit(1);
    } else {
        // Parent process
        int status;
        waitpid(pid, &status, 0);
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            fprintf(stderr, "tar completed \n");
        }
    }
}


int parse_and_Execute_command(char *command, int new_socket) {
    char *cmd = strtok(command, " \n");
    char commandBuffer[4096];

    if (!cmd) {
        printf("Invalid command\n");
        return -2;
    }
    if (strcmp(cmd, "findfile") == 0) {
        char *filename = strtok(NULL, " \n");
        if (!filename) {
            printf("Usage: findfile filename\n");
            return -2;
        }
        // char *file_name = strtok(buffer + 9, "\n");
        char file_info[4096];
        memset(file_info, 0, sizeof(file_info));
        
        off_t file_size = -1;
        time_t creation_time = -1;
        // search_file(getenv("HOME"), filename, &file_size, &creation_time);
        int status = find_file(getenv("HOME"), filename, file_info);
        if (status == 0) {
            // File not found
            memset(file_info, 0, sizeof(file_info));
            snprintf(file_info, sizeof(file_info), "File not found");
            send(new_socket, file_info, strlen(file_info), 0);
            printf("Client: File not found\n");
        } else {
            // Send the file info to the client
            send(new_socket, file_info, strlen(file_info), 0);
            printf("Client: %s\n", file_info);
            memset(file_info, 0, sizeof(file_info));
        }
        return 1;
    } else if (strcmp(cmd, "sgetfiles") == 0) {
        char *size1_str = strtok(NULL, " \n");
        char *size2_str = strtok(NULL, " \n");
        if (!size1_str || !size2_str) {
            // printf("Usage: sgetfiles size1 size2 [-u]\n");
            return -2;
        }
        long size1 = atol(size1_str);
        long size2 = atol(size2_str);
        if (size1 <= 0 || size2 <= 0 || size1 > size2) {
            // printf("Invalid size range\n");
            return -2;
        }
        memset(commandBuffer, 0, sizeof(commandBuffer));
       // char *commandBuffer = "find . -type f -size +999 -size -10001";
        snprintf(commandBuffer, sizeof(commandBuffer), "find %s -type f -size +%ld -size -%ld",getenv("HOME"), size1, size2);
        
        handleFileTar(commandBuffer);
        return 0;

    } else if (strcmp(cmd, "dgetfiles") == 0) {
        char *date1_str = strtok(NULL, " \n");
        char *date2_str = strtok(NULL, " \n");
        if (!date1_str || !date2_str) {
            printf("Usage: dgetfiles date1 date2 [-u]\n");
            return -2;
        }
        memset(commandBuffer, 0, sizeof(commandBuffer));
        // char *commandBuffer = "find ~ -type f -newermt date1 ! -newermt date2";
        snprintf(commandBuffer, sizeof(commandBuffer), "find %s -type f -newermt %s ! -newermt %s",getenv("HOME"), date1_str, date2_str);

        handleFileTar(commandBuffer);
        return 0;
    } else if (strcmp(cmd, "getfiles") == 0) {
        // printf("getfiels called");
        char *filename = strtok(NULL, " \n");
        int count = 0;
        char args[1024];
        memset(args, 0, sizeof(args));
        while (filename && count < 6) {
            if(strcmp(filename, "-u") == 0)
            {
                count++;
                continue;
            }
            if(count != 0)
                strcat(args, " -o ");
            strcat(args, " -name '");
            strcat(args, filename);
            strcat(args, "'");
            count++;
            filename = strtok(NULL, " \n");

        }

        if (count == 0) {
            printf("Usage: getfiles file1 [file2 ... file6] [-u]\n");
            return -2;
        }
        memset(commandBuffer, 0, sizeof(commandBuffer));
        // char *commandBuffer = "find ~ -type f -newermt date1 ! -newermt date2";
        snprintf(commandBuffer, sizeof(commandBuffer), "find %s %s",getenv("HOME"), args);

        handleFileTar(commandBuffer);
        return 0;
    } else if (strcmp(cmd, "gettargz") == 0) {
        char *ext = strtok(NULL, " \n");
        int count = 0;
        char args[1024];
        memset(args, 0, sizeof(args));
        while (ext && count < 6) {
            if(strcmp(ext, "-u") == 0)
            {
                count++;
                continue;
            }
            if(count != 0)
                strcat(args, " -o ");
            strcat(args, " -name '*.");
            strcat(args, ext);
            strcat(args, "'");
            count++;
            ext = strtok(NULL, " \n");
        }
        if (count == 0) {
            printf("Usage: gettargz extension1 [extension2 ... extension6] [-u]\n");
            return -2;
        }
        memset(commandBuffer, 0, sizeof(commandBuffer));
        // char *commandBuffer = "find ~ -type f -newermt date1 ! -newermt date2";
        snprintf(commandBuffer, sizeof(commandBuffer), "find %s %s",getenv("HOME"), args);

        handleFileTar(commandBuffer);       
        return 0;
    } else if (strcmp(cmd, "quit") == 0) {
        return -2;
    }
    printf("Invalid Command\n");
    return -2;
}


void process_client (int sockfd)
{
    char buffer[1024] = {0};
    char tempBuff[1024] = {0};
    int valread;
    // Reading messages from client
    while(1)
    {
        FILE *fp;
        int file_size = 0;
        int bytes_sent = 0;
        memset(buffer, 0, sizeof(buffer));
        memset(tempBuff, 0, sizeof(tempBuff));

        valread = read(sockfd, buffer, 1024);
        printf("command received: %s", buffer);
        strcpy(tempBuff, buffer);
        char* filename = "temp.tar.gz";

        int status = parse_and_Execute_command(tempBuff, sockfd);
        FILE *fp1 = fopen("temp.txt", "r");
        int size;
        fseek(fp1, 0L, SEEK_END);
        size = ftell(fp1);
        fclose(fp1);
        if(size == 0) // FILE not found
        {
            sprintf(buffer, "%s %d", "b_failed_no_file_found", 0);
            send(sockfd, buffer, strlen(buffer), 0);
        }
        if((status == -2) || (strcmp(buffer, "exit\n") == 0))
        {
            printf("Client disconnected\n");
            break;
        }
        if(status == 0)
        {        
            // open file for reading
            int file_fd, file_size;

            if ((file_fd = open(filename, O_RDONLY)) < 0) {
                perror("file open failed");
                exit(EXIT_FAILURE);
            }

            // get file size
            if ((file_size = lseek(file_fd, 0, SEEK_END)) < 0) {
                perror("file size failed");
                exit(EXIT_FAILURE);
            }
            lseek(file_fd, 0, SEEK_SET);

            // send file size to client
            sprintf(buffer, "%s %d", filename, file_size);
            send(sockfd, buffer, strlen(buffer), 0);
            // printf("sending file: %s of size : %d", filename, file_size);
            // send file data to client
            int bytes_sent = 0, bytes_read;
            while (bytes_sent < file_size) {
                // printf("sending chunk...");
                bytes_read = read(file_fd, buffer, BUFFER_SIZE);
                if (bytes_read < 0) {
                    perror("read failed");
                    exit(EXIT_FAILURE);
                }
                // printf("%s",buffer);
                send(sockfd, buffer, bytes_read, 0);
                // printf("send in bytes : %d of size : %d", bytes_sent, file_size);
                bytes_sent += bytes_read;
            }

            // close file
            close(file_fd);
        }
        memset(buffer, 0, sizeof(buffer));
        memset(tempBuff, 0, sizeof(tempBuff));
    }
}



int main(int argc, char const *argv[])
{
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    char *welcome_message = "Welcome to the Mirror File Server\n";
    
    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket creation error");
        exit(EXIT_FAILURE);
    }
    
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
       
    // Binding socket to the specified port
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address))<0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    
    // Listening for incoming connections
    if (listen(server_fd, 3) < 0)
    {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }
    
    while(1)
    {
        // Accepting incoming connections
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen))<0)
        {
            perror("accept failed");
            exit(EXIT_FAILURE);
        }
        
        int pid = fork();
        if (pid < 0) {
            perror("fork failed");
            close(new_socket);
            exit(EXIT_FAILURE);
        } else if (pid == 0) {
            // Child process handles client requests
            printf("New client connected to Mirror\n");
            // Sending welcome message to client
            close(server_fd);
            process_client(new_socket);
            close(new_socket);
            exit(0);
        } 
        close(new_socket);
    }
    return 0;
}

