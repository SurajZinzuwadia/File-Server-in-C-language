#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#define PORT 8080
#define MIRROR_PORT 9001
#define BUFFER_SIZE 1024

/*
parameter: char date
Given function validate the individual date
*/
int check_valid_date(char* date) {
    // Check that the date has the format YYYY-MM-DD
    int year, month, day;
    if (sscanf(date, "%d-%d-%d", &year, &month, &day) != 3) {
        return 0; // if true
    }
    if (year < 1 || year > 9999 || month < 1 || month > 12 || day < 1 || day > 31) {
        return 0;
    }
    return 1; // if false
}

/*
parameter: char date1
           chat date2
Given function validate that date1 must be less than date2
*/
int dateIsLessThan(char* date1, char* date2) {
    int year1, month1, day1, year2, month2, day2;
    sscanf(date1, "%d-%d-%d", &year1, &month1, &day1);
    sscanf(date2, "%d-%d-%d", &year2, &month2, &day2);
    
    if (year1 < year2) {
        return 0;
    } else if (year1 == year2 && month1 < month2) {
        return 0;
    } else if (year1 == year2 && month1 == month2 && day1 < day2) {
        return 0;
    } else {
        return 1;
    }
}


/*
parameter: char filelist
Given function validate filelist
*/
int check_valid_filelist(char* filelist) {
    // Check that the file list contains at least one file
    if (strlen(filelist) == 0) {
        return 0;
    }
    return 1;
}



/*
parameter: char extensions
Given function validate extensions
*/
int check_valid_extensions(char* extensions) {
    // Check that the extensions list contains at least one extension
    if (strlen(extensions) == 0) {
        return 0;
    }
    return 1;
}



/*
parameter: char *command
            int uUnzip
            int uRecvFile
Given function parse and validate the command given from user
*/
int parse_and_validate_command(char *command, int *uUnzip, int *uRecvFile) {
    // command takes string from user input ex. findfile sample.txt
    char *cmd = strtok(command, " \n"); // fetch the first word
    if (!cmd) { // if not cmd then return -1
        printf("Invalid command\n");
        return -1;
    }
    if (strcmp(cmd, "findfile") == 0) { 
        char *filename = strtok(NULL, " \n"); // fetch the second word
        //if not filename return the format
        if (!filename) {
            printf("Usage: findfile filename\n");
            return -1;
        }
        *uRecvFile = 1;  // not receiving the files from server so flag will be 1

        return 0; // return true
    } else if (strcmp(cmd, "sgetfiles") == 0) {
        char *size1_str = strtok(NULL, " \n"); // fetch size1 from command
        char *size2_str = strtok(NULL, " \n"); // fetch size2 from command
        //return valid format if not
        if (!size1_str || !size2_str ) {
            printf("Usage: sgetfiles size1 size2 [-u]\n");
            return -1;
        }
        int size1 = atoi(size1_str); // convert to int
        int size2 = atoi(size2_str); // convert to int
        // validation of both size
        if (size1 <= 0 || size2 <= 0 || size1 > size2) {
            printf("Invalid size range\n");
            return -1;
        }
        char *unzip = strtok(NULL, " \n"); //fetch if -u in command
        if (unzip && strcmp(unzip, "-u") == 0) {
            //update the flag if -u recevied
            *uUnzip = 0;
        }
        //update the receive file flag
        *uRecvFile = 0;
        return 0;

    } else if (strcmp(cmd, "dgetfiles") == 0) {
        char *date1_str = strtok(NULL, " \n"); //fetch date1
        char *date2_str = strtok(NULL, " \n"); // fetch date2
        //validate dates
        if (!date1_str || !date2_str || !check_valid_date(date1_str) || !check_valid_date(date2_str)) {
            printf("Usage: dgetfiles date1 date2 [-u]\n");
            return -1;
        }
        if(dateIsLessThan(date1_str, date2_str))
        {
            printf("Error:date1 is greater than date2\n");
            return -1;
        }
        char *unzip = strtok(NULL, " \n");
        if (unzip && strcmp(unzip, "-u") == 0) { // check if -u recevied
            *uUnzip = 0;
        }
        *uRecvFile = 0; // update received file flag to 0
        return 0;
    } else if (strcmp(cmd, "getfiles") == 0) {
        char *filename = strtok(NULL, " \n"); // get filenames
        int count = 0;
        while (filename && count < 6) {
            if(strcmp(filename, "-u") == 0)
            {
                *uUnzip = 0; // update flag if -u received
                count++;
                continue;
            }
            count++;

            filename = strtok(NULL, " \n"); // get new filename
            
        }
        if (count == 0) {
            printf("Usage: getfiles file1 [file2 ... file6] [-u]\n");
            return -1;
        }
        char *unzip = strtok(NULL, " \n");
        // update -u flag
        if (unzip && strcmp(unzip, "-u") == 0) {
            *uUnzip = 0;
        }
        //update recevied file flag
        *uRecvFile = 0;
        return 0;
    } else if (strcmp(cmd, "gettargz") == 0) {
        char *ext = strtok(NULL, " \n"); // get extension name
        int count = 0;
        while (ext && count < 6) {
            if(strcmp(ext, "-u") == 0)
            {
                *uUnzip = 0; // -u flag updated
                count++;
                continue;
            }
            count++;
            ext = strtok(NULL, " \n");
        }
        if (count == 0) {
            printf("Usage: gettargz extension1 [extension2 ... extension6] [-u]\n");
            return -1;
        }
        char *unzip = strtok(NULL, " \n");
        if (unzip && strcmp(unzip, "-u") == 0) {
         *uUnzip = 0;
        }
        *uRecvFile = 0;
        return 0;
    } else if (strcmp(cmd, "quit") == 0) { // if quit then exit the from server
        return -2;
    }
    printf("Invalid Command\n");
    return -1;
}

int main(int argc, char const *argv[])
{
    // add required variables
    int sock = 0, valread;
    struct sockaddr_in serv_addr, mirror_addr;
    char buffer[1024] = {0}; // main buffer
    char tempbuf[1024] = {0}; // temp buffer to parse and validate
    char filename[1024] = {0};

   
   
    // Creating socket file descriptor
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket creation error");
        exit(EXIT_FAILURE);
    }
    //clear the ser_addr 
    memset(&serv_addr, '0', sizeof(serv_addr));
   
   //required connection
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
       
    // Converting IPv4 and IPv6 addresses from text to binary form
    //127.0.0.1 localhost
    if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr)<=0)
    {
        perror("invalid address/ address not supported");
        exit(EXIT_FAILURE);
    }
    // Connecting to the server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("connection failed");
        exit(EXIT_FAILURE);
    }
    //created response buffer
    char response[1024]={0};
    valread = read(sock, response, 1024); // read from server and store into response
    response[strcspn(response, "\n")] = '\0';
    if (strcmp(response, "9001") == 0) // if response is 9001 converting to mirror
    {
        printf("Conneted to Mirror");
        close(sock); // close the current socket
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if(sock == -1)
        {
            perror("socket creation error");
            exit(EXIT_FAILURE);
        }
        //clear the mirror_addr
        memset(&mirror_addr, '0', sizeof(mirror_addr));
        mirror_addr.sin_family = AF_INET;
        mirror_addr.sin_port = htons(MIRROR_PORT);
        if(inet_pton(AF_INET, "127.0.0.1", &mirror_addr.sin_addr)<=0)
        {
            perror("invalid address/ address not supported");
            exit(EXIT_FAILURE);
        }
        // Connecting to the mirror
        if (connect(sock, (struct sockaddr *)&mirror_addr, sizeof(mirror_addr)) < 0)
        {
            perror("connection failed");
            exit(EXIT_FAILURE);
        }
    }
    // Sending messages to server or mirror based on the connection
    while(1)
    {
        //clear buffer and tempbuf
        memset(buffer, 0, sizeof(buffer));
        memset(tempbuf, 0, sizeof(tempbuf));
        printf("\nEnter Command: ");
        fgets(buffer, 1024, stdin); //read  user input
        int uUnzip = 1; // unzip bool initially 1
        printf("client : %s",buffer);
        strcpy(tempbuf, buffer);

        int uRecvFile = 1; //received bool initially 1
        //return int in status after parse and validate
        int status = parse_and_validate_command(tempbuf, &uUnzip, &uRecvFile);
        // return 0 is successful execution
        if(status == 0)
        {
            printf("sending command: %s", buffer);
            send(sock, buffer, strlen(buffer), 0);
        }
        //return -2 termination from program
        else if (status == -2 )
        {
            send(sock, buffer, strlen(buffer), 0);

            printf("Disconnected from the server\n");
            break;
        }else // else will continue the loop if invalid command executed (return -1)
        {
            continue;
        }

        //after sending command
        memset(buffer, 0, sizeof(buffer));
        memset(tempbuf, 0, sizeof(tempbuf));
        memset(filename, 0, sizeof(filename));
        if(uRecvFile == 0)
        {

            int bytes_received = 0;
            // Open file for writing
            FILE *fp;
                // receive file name and size from server
             recv(sock, buffer, BUFFER_SIZE, 0);
            char *file_name = strtok(buffer, " ");
            int file_size = atoi(strtok(NULL, " "));
            if(strcmp(file_name, "b_failed_no_file_found") == 0 && file_size == 0)
            {
                printf("File not found");
                memset(buffer, 0, BUFFER_SIZE);
                memset(filename, 0, BUFFER_SIZE);
                continue;
            }
            // printf("\nreceiving file %s of size %d",file_name,  file_size);
            // open file for writing
            fp = fopen(file_name, "wb");
            if (fp == NULL) {
                perror("file open failed");
                exit(EXIT_FAILURE);
            }

            int count = 0;
            // receive file data from server
            while (bytes_received < file_size) {
                int bytes_read = recv(sock, buffer, BUFFER_SIZE, 0);
                fwrite(buffer, sizeof(char), bytes_read, fp);
                bytes_received += bytes_read;
                printf("\n i = %d bytes received = %d total size = %d",count,bytes_received,file_size);
            }

        // Close file
        fclose(fp);
        printf("File received and saved temp.tar.gz \n");

        if(uUnzip == 0)
        {
            system("tar -xvf temp.tar.gz");
            printf("temp.tar.gz is unziped\n");
            uUnzip = 1;
        }

        // Clear buffer and filename for next file
        memset(buffer, 0, BUFFER_SIZE);
        memset(filename, 0, BUFFER_SIZE);
        }else
        {
            memset(buffer, 0, sizeof(buffer));
            valread = read(sock, buffer, 1024);
            // print("Buffer in else: %s",)

            printf("Server: %s", buffer);
        }
    }
    close(sock);
    return 0;
}
