#include <stdio.h>
#include <dirent.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <openssl/md5.h>
#include <openssl/hmac.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_PACKET_LENGTH 10240

typedef struct print_data{
    char filename[100]; //filename
    off_t size;         //size
    time_t mtime;       //last modified
    char type;          //filetype
}print_data;

typedef struct print_hash{
    char *filename;                        //filename
    unsigned char hash[MD5_DIGEST_LENGTH]; //hash
    time_t mtime;                          //last modified
}print_hash;

   int parse_request(char *request);
  void IndexGet(char *request);
  void FileDownload(char *request);
   int longlist();
   int shortlist(time_t start_time,time_t end_time);
   int checkall();
  void FileHash(char *request);
   int verify(char *file);
  char con[5];
   int i, hist_count = 0, error = -1;
  char history[1024][1024] = {0};
  char fileDownloadName[1024];
  char response[MAX_PACKET_LENGTH];
   int regexpression = 0;
    int udpclientflagset = 0;

print_data pdata[1024];
print_hash hdata[1024];

int main(int argc,char *argv[])
{
    if(argc != 5)
    {
    printf("Incorrect format\n");
        printf("\n Usage: %s <listenportno> <ip of server> <connectportno> <protocol>\n", argv[0]);
        return 1;
    } 
    char *listenportno = argv[1];
    char *connectportno = argv[3];
    char *ip = argv[2];
    if(strcmp(argv[3],"udp")==0)
        udpclientflagset = 1;
    else
        udpclientflagset = 0;
    //pid_t pid;
    //Create a copy of the current process using fork
    int pid = fork();
    strcpy(con,argv[4]);
    //Depending on supplied arguements, use the process copy
    if(pid == 0)
    {
        if(udpclientflagset==1)
            act_as_udp_server(listenportno);
        else
            act_as_tcp_server(listenportno);
    }
    else if(pid > 0)
    {        
        if(udpclientflagset==1)
            act_as_udp_client(ip,connectportno);
        else
            act_as_tcp_client(ip,connectportno);
    }
    return 0;
}

int parse_request(char *request)
{
    char copyrequest[100];
    strcpy(copyrequest,request);
    char *request_data = NULL;
    const char delim[] = " \n";
    request_data = strtok(copyrequest,delim);
    if(request_data != NULL)
    {   
        if(strcmp(request_data,"IndexGet") == 0)
            return 1;
        else if(strcmp(request_data,"FileHash") == 0)
            return 2;
        else if(strcmp(request_data,"FileDownload") == 0)
            return 3;
        else if(strcmp(request_data,"FileUpload") == 0)
            return 4;
        else if(strcmp(request_data, "Quit") == 0 || strcmp(request_data, "Q") == 0 || strcmp(request_data, "quit") == 0 || strcmp(request_data, "q") == 0 || strcmp(request_data, "exit") == 0 || strcmp(request_data, "Exit") == 0 )
            return 5;
        else
            return -1;
    }
}

void IndexGet(char *request)
{
    char *request_data = NULL;
    char delim[] = " \n";
    struct tm tm;
    time_t start_time , end_time;
    int enter = 1;
    char *regexp;

    request_data = strtok(request,delim);
    request_data = strtok(NULL,delim);
    if(request_data == NULL)
    {
        error = 1;
        sprintf(response,"ERROR: Wrong Format.\n Correct Formats : (1)IndexGet<space>LongList\n(2)IndexGet<space>ShortList<space>StartTimeStamp<space>EndTimeStamp\n");
        return;
    }
    else
    {
        if(strcmp(request_data,"LongList") == 0)
        {
            request_data = strtok(NULL,delim);

            if(request_data != NULL)
            {
                printf("Entered in error\n");
                error = 1;
                sprintf(response,"ERROR: Wrong Format. The correct format is:\nIndexGet LongList\n");
                return;
            }
            else
            {
                longlist();
            }
        }
        else if(strcmp(request_data,"ShortList") == 0)
        {
            request_data = strtok(NULL,delim);
            if(request_data == NULL)
            {
                error = 1;
                sprintf(response,"ERROR: Wrong Format. The correct format is:\nIndexGet ShortList <timestamp1> <timestamp2>\n");
                return;
            }
            while(request_data != NULL)
            {
                if(enter >= 3)
                {
                    error = 1;
                    sprintf(response,"ERROR: Wrong Format. The correct format is:\nIndexGet ShortList <timestamp1> <timestamp2>\n");
                    return;
                }
                if (strptime(request_data, "%d-%b-%Y-%H:%M:%S", &tm) == NULL)
                {
                    error = 1;
                    sprintf(response,"ERROR: Wrong Format. The correct format is:\nDate-Month-Year-hrs:min:sec\n");
                    return;
                }
                if(enter == 1)
                    start_time = mktime(&tm);
                else
                    end_time = mktime(&tm);
                enter++;
                request_data = strtok(NULL,delim);
            }
            shortlist(start_time,end_time);
        }
        else if(strcmp(request_data,"regex") == 0)
        {
            request_data = strtok(NULL,delim);
            if(request_data == NULL)
            {
                printf("ERROR: Wrong Format. The correct format is:\nIndexGet regex <regular expression>\n");
                _exit(1);
            }
            regexp = request_data;
            request_data = strtok(NULL,delim);
            if(request_data != NULL)
            {
                printf("ERROR: Wrong Format. The correct format is:\nIndexGet regex <regular expression>\n");
                _exit(1);
            }
            regex(regexp);
        }
        else
        {
            error = 1;
            sprintf(response,"ERROR: Wrong Format.\n");
            return;
        }
    }
}

void FileDownload(char *request)
{
    char *request_data = NULL;
    char delim[] = " \n";
    request_data = strtok(request,delim);
    request_data = strtok(NULL,delim);
    if(request_data == NULL)
    {
        error = 1;
        sprintf(response,"ERROR: Wrong Format. The correct format is:\nFileDownload <file_name>\n");
        return;
    }
    strcpy(fileDownloadName,request_data);
    request_data = strtok(NULL,delim);
    if(request_data != NULL)
    {
        error = 1;
        sprintf(response,"ERROR: Wrong Format. The correct format is:\nFileDownload <file_name>\n");
        return;
    }
}

int longlist()
{
    DIR *dp;
    i = 0; 
    struct dirent *ep;
    dp = opendir ("./");
    struct stat fileStat;

    if (dp != NULL)
    {
        while (ep = readdir (dp))
        {
            if(stat(ep->d_name,&fileStat) < 0)
                return 1;
            else
            {
                strcpy(pdata[i].filename, ep->d_name);
                pdata[i].size = fileStat.st_size;
                pdata[i].mtime = fileStat.st_mtime;
                pdata[i].type = (S_ISDIR(fileStat.st_mode)) ? 'd' : '-';
                i++;
            }
        }
        closedir (dp);
    }
    else
    {
        printf("Couldn't open the directory");
    }
}

int shortlist(time_t start_time,time_t end_time)
{
    DIR *dp;
    i = 0;
    struct dirent *ep;
    dp = opendir ("./");
    struct stat fileStat;

    if (dp != NULL)
    {
        while (ep = readdir (dp))
        {
            if(stat(ep->d_name,&fileStat) < 0)
                return 1;
            else if(difftime(fileStat.st_mtime,start_time) > 0 && difftime(end_time,fileStat.st_mtime) > 0)
            {
                strcpy(pdata[i].filename , ep->d_name);
                pdata[i].size = fileStat.st_size;
                pdata[i].mtime = fileStat.st_mtime;
                pdata[i].type = (S_ISDIR(fileStat.st_mode)) ? 'd' : '-';
                i++;
            }
        }
        closedir (dp);
    }
    else
    {
        error = 1;
        sprintf(response,"Couldn't open the directory");
    }
}

int regex(char *regexp)
{
    FILE *pipein_fp;
    char string[1024] = "ls ";
    char str[1024];
    memset(str, 0,sizeof(str));
    char line[1024];
    char readbuf[1024];
    i = 0;
    regexpression = 1;

    strncpy(str,regexp+1,strlen(regexp)-2);
    strcat(string,str);

    DIR *dp;
    int a;
    struct dirent *ep;
    dp = opendir ("./");
    struct stat fileStat;

    if (dp != NULL)
    {
        while (ep = readdir (dp))
        {
            if(stat(ep->d_name,&fileStat) < 0)
                return 1;
            else
            {
                if (( pipein_fp = popen(string, "r")) == NULL)
                {
                    perror("popen");
                    exit(1);
                }
                while(fgets(readbuf, 1024, pipein_fp))
                {
                    strncpy(line,readbuf,strlen(readbuf)-1);
                    if(strcmp(line,ep->d_name) == 0)
                    {
                        strcpy(pdata[i].filename , ep->d_name);
                        pdata[i].size = fileStat.st_size;
                        pdata[i].type = (S_ISDIR(fileStat.st_mode)) ? 'd' : '-';
                        i++;
                        break;
                    }
                    memset(line, 0,sizeof(line));
                }
            }
        }

        pclose(pipein_fp);
    }
    else
    {
        error = 1;
        sprintf(response,"Couldn't open the directory");
    }
}

void FileHash(char *request)
{
    char *request_data = NULL;
    char delim[] = " \n";
    request_data = strtok(request,delim);
    request_data = strtok(NULL,delim);
    if(request_data == NULL)
    {
        error = 1;
        sprintf(response,"ERROR: Wrong Format\n");
    }
    while(request_data != NULL)
    {
        if(strcmp(request_data,"CheckAll") == 0)
        {
            request_data = strtok(NULL,delim);

            if(request_data != NULL)
            {
                error = 1;
                sprintf(response,"ERROR: Wrong Format. The correct format is:\nFileHash CheckAll\n");
                return;
            }
            else
            {
                checkall();
            }

        }
        else if(strcmp(request_data,"Verify") == 0)
        {
            request_data = strtok(NULL,delim);
            if(request_data == NULL)
            {
                error = 1;
                sprintf(response,"ERROR: Wrong Format. The correct format is:\nFileHash Verify <filename>\n");
                return;
            }
            char *filename = request_data;
            request_data = strtok(NULL,delim);
            if(request_data != NULL)
            {
                error = 1;
                sprintf(response,"ERROR: Wrong Format. The correct format is:\nFileHash Verify <filename>\n");
                return;
            }
            else
                verify(filename);
        }
    }
}

int checkall()
{
    unsigned char c[MD5_DIGEST_LENGTH];
    DIR *dp;
    i = 0;
    int a;
    struct dirent *ep;
    dp = opendir ("./");
    struct stat fileStat;

    if (dp != NULL)
    {
        while (ep = readdir (dp))
        {
            if(stat(ep->d_name,&fileStat) < 0)
                return 1;
            else
            {
                char *filename=ep->d_name;
                hdata[i].filename = ep->d_name;
                hdata[i].mtime = fileStat.st_mtime;
                FILE *inFile = fopen (filename, "r");
                MD5_CTX mdContext;
                int bytes;
                unsigned char data[1024];

                if (inFile == NULL) {
                    error = 1;
                    sprintf (response,"%s can't be opened.\n", filename);
                    return 0;
                }

                MD5_Init (&mdContext);
                while ((bytes = fread (data, 1, 1024, inFile)) != 0)
                    MD5_Update (&mdContext, data, bytes);
                MD5_Final (c,&mdContext);
                for(a = 0; a < MD5_DIGEST_LENGTH; a++)
                    hdata[i].hash[a] = c[a];
                fclose (inFile);
                i++;
            }
        }
    }
    else
    {
        error = 1;
        sprintf(response,"Couldn't open the directory");
    }
}

int verify(char *file)
{
    unsigned char c[MD5_DIGEST_LENGTH];
    DIR *dp;
    i = 0;
    int a;
    struct dirent *ep;
    dp = opendir ("./");
    struct stat fileStat;

    if (dp != NULL)
    {
        while (ep = readdir (dp))
        {
            if(stat(ep->d_name,&fileStat) < 0)
                return 1;
            else if(strcmp(file,ep->d_name) == 0)
            {
                char *filename=ep->d_name;
                hdata[i].filename = ep->d_name;
                hdata[i].mtime = fileStat.st_mtime;
                FILE *inFile = fopen (filename, "r");
                MD5_CTX mdContext;
                int bytes;
                unsigned char data[1024];

                if (inFile == NULL) {
                    error = 1;
                    sprintf(response,"%s can't be opened.\n", filename);
                    return 0;
                }

                MD5_Init (&mdContext);
                while ((bytes = fread (data, 1, 1024, inFile)) != 0)
                    MD5_Update (&mdContext, data, bytes);
                MD5_Final (c,&mdContext);
                for(a = 0; a < MD5_DIGEST_LENGTH; a++)
                    hdata[i].hash[a] = c[a];
                fclose (inFile);
                i++;
            }
            else
                continue;
        }
    }
    else
    {
        error = 1;
        sprintf(response,"Couldn't open the directory");
    }
}

int act_as_tcp_server(char *listenportno)
{
    int listenfd = 0;  //Listener File Descriptor
    int connfd = 0;    //Connection File Descriptor
    struct sockaddr_in serv_addr; 
    int portno = atoi(listenportno);

    char readBuff[1024];
    char writeBuff[1024];
    time_t ticks; 

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if(listenfd == -1)
    {
        perror("Unable to create socket");
        exit(0);
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    memset(readBuff, 0, sizeof(readBuff)); 
    memset(writeBuff, 0, sizeof(writeBuff)); 

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(portno); 


    if(bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
    {
        perror("Unable to bind");
        exit(0);
    }

    listen(listenfd, 10); 
    connfd = accept(listenfd, (struct sockaddr*)NULL, NULL); 

    int a , n , b , c;
    n = read(connfd,readBuff,sizeof(readBuff));


    while(n > 0)
    {
        size_t size = strlen(readBuff) + 1;
        char *request = malloc(size);
        strcpy(request,readBuff);

        strcpy(history[hist_count],readBuff);
        hist_count++;

        int type_request = parse_request(request);
        printf("\nCommand Received : %s",request);
        response[0] = '\0';
        writeBuff[0] = '\0';
        printf("[Enter Command Here : ] ");
        fflush(stdout);
        if(type_request == -1)      //Error
        {
            error = 1;
            sprintf(response,"ERROR: No request of this type.\n");
        }
        else if(type_request == 1)      //Indexget
            IndexGet(request);
        else if(type_request == 2)      //FileHash
            FileHash(request);
        else if(type_request == 3)      //FileDownload
            FileDownload(request);

        if(error == 1)
        {
            strcat(writeBuff,response);
            strcat(writeBuff,"~@~");
            write(connfd , writeBuff , strlen(writeBuff));
            error = -1;
            memset(writeBuff, 0, sizeof(writeBuff));
            memset(readBuff, 0, sizeof(readBuff)); 
            while((n = read(connfd,readBuff,sizeof(readBuff)))<=0);
            continue;
        }
        if(type_request == 1 && regexpression == 1)
        {
            strcat(writeBuff,response);

            for(a = 0 ; a < i ; a++)
            {
                sprintf(response, "%-35s| %-10d| %-3c| %-20s" , pdata[a].filename , pdata[a].size , pdata[a].type , ctime(&pdata[a].mtime));
                strcat(writeBuff,response);
            }
            strcat(writeBuff,"~@~");

            write(connfd , writeBuff , strlen(writeBuff));
        }
        else if(type_request == 1)
        {
            strcat(writeBuff,response);

            for(a = 0 ; a < i ; a++)
            {
                sprintf(response, "%-35s| %-10d| %-3c| %-20s" , pdata[a].filename , pdata[a].size , pdata[a].type , ctime(&pdata[a].mtime));
                strcat(writeBuff,response);
            }
            strcat(writeBuff,"~@~");

            write(connfd , writeBuff , strlen(writeBuff));
        }
        else if(type_request == 2)
        {
            strcat(writeBuff,response);

            for (b = 0 ; b < i ; b++)
            {
                sprintf(response, "%-35s |   ",hdata[b].filename);
                strcat(writeBuff,response);
                for (c = 0 ; c < MD5_DIGEST_LENGTH ; c++)
                {
                    sprintf(response, "%x",hdata[b].hash[c]);
                    strcat(writeBuff,response);
                }
                sprintf(response, "\t %20s",ctime(&hdata[b].mtime));
                strcat(writeBuff,response);
            }
            strcat(writeBuff,"~@~");

            write(connfd , writeBuff , strlen(writeBuff));
        }
        else if(type_request == 3)
        {
            FILE* fp;
            fp = fopen(fileDownloadName,"rb");
            size_t bytes_read;
            while(!feof(fp))
            {
                bytes_read = fread(response, 1, 1024, fp);
                memcpy(writeBuff,response,bytes_read);
                write(connfd , writeBuff , bytes_read);
                memset(writeBuff, 0, sizeof(writeBuff));
                memset(response, 0, sizeof(response));
            }
            memcpy(writeBuff,"~@~",3);
            write(connfd , writeBuff , 3);
            memset(writeBuff, 0, sizeof(writeBuff));
            fclose(fp);
        }
        else if(type_request == 4)
        {
            printf("FileUpload Accepted\n");
            memcpy(writeBuff,"FileUpload Accept\n",18);
            write(connfd , writeBuff , 18);
            memset(writeBuff, 0,18);
            char copyrequest[1024];
            memset(copyrequest, 0,1024);
            memcpy(copyrequest,request,1024);
            char *size = strtok(copyrequest,"\n");
            size = strtok(NULL,"\n");
            long fsize = atol(size);
            char *request_data = NULL;
            const char delim[] = " \n";
            request_data = strtok(request,delim);
            request_data = strtok(NULL,delim);
            int f;
            int result;
            f = open(request_data, O_WRONLY | O_CREAT | O_EXCL, (mode_t)0600);
            if (f == -1) {
                perror("Error opening file for writing:");
                return 1;
            }
            result = lseek(f,fsize-1, SEEK_SET);
            result = write(f, "", 1);
            if (result < 0) {
                close(f);
                perror("Error opening file for writing:");
                return 1;
            }
            close(f);
            FILE *fp;
            fp = fopen(request_data,"wb");
            n = read(connfd, readBuff, sizeof(readBuff)-1);
            while(1)
            {
                readBuff[n] = 0;
                if(readBuff[n-1] == '~' && readBuff[n-3] == '~' && readBuff[n-2] == '@')
                {
                    readBuff[n-3] = 0;
                    fwrite(readBuff,1,n-3,fp);
                    fclose(fp);
                    memset(readBuff, 0,n-3);
                    break;
                }
                else
                {
                    fwrite(readBuff,1,n,fp);
                    memset(readBuff, 0,n);
                }
                n = read(connfd, readBuff, sizeof(readBuff)-1);
                if(n < 0)
                    break;
            }
            memset(writeBuff, 0,1024);

        }

        regexpression = 0;
        memset(readBuff, 0, sizeof(readBuff)); 
        memset(writeBuff, 0, sizeof(writeBuff));
        while((n = read(connfd,readBuff,sizeof(readBuff)))<=0);
    }
    close(connfd);
    wait(NULL);
}

int act_as_tcp_client(char *ip,char *connectportno)
{
    int sockfd = 0, n = 0;
    char readBuff[1024];
    char writeBuff[1024];
    struct sockaddr_in serv_addr;
    int portno = atoi(connectportno);
    char DownloadName[1024];
    char UploadName[1024];

    memset(readBuff, 0,sizeof(readBuff));
    memset(writeBuff, 0,sizeof(writeBuff));
    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("\n Error : Could not create socket \n");
        return 1;
    } 

    memset(&serv_addr, 0, sizeof(serv_addr)); 

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(portno); 

    if(inet_pton(AF_INET, ip, &serv_addr.sin_addr)<=0)
    {
        printf("\n inet_pton error occured\n");
        return 1;
    } 

    while(1)
    {
        if( connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        {
            continue;
        }
        else
        {
        if(strcmp(con,"udp"))
            printf("Client is Connected now : \n");
            break;
        }
    }

    int a , count = 0 , filedownload = 0 , fileupload = 0;

 
    while(1)
    {

    printf("[Enter Command Here : ]");
        filedownload = 0;
        fileupload = 0;
        FILE *fp = NULL;
        int i;
        char *cresponse = malloc(MAX_PACKET_LENGTH);
        fgets(writeBuff,sizeof(writeBuff),stdin);

        char *filename;
        char copy[1024];
        strcpy(copy,writeBuff);
        filename = malloc(1024);
        filename = strtok(copy," \n");
        if(strcmp(filename,"quit") == 0)
        _exit(1);
        if(strcmp(filename,"FileDownload") == 0)
        {
            filedownload = 1;
            filename = strtok(NULL," \n");
            strcpy(DownloadName,filename);
            fp = fopen(DownloadName,"wb");
        }
        if(strcmp(filename,"FileUpload") == 0)
        {
            fileupload = 1;
            filename = strtok(NULL," \n");
            strcpy(UploadName,filename);
            FILE *f = fopen(UploadName, "r");
            fseek(f, 0, SEEK_END);
            unsigned long len = (unsigned long)ftell(f);
            char size[1024];
            memset(size, 0, sizeof(size));
            sprintf(size,"%ld\n",len);
            strcat(writeBuff,size);
            fclose(f);
        }
        write(sockfd, writeBuff , strlen(writeBuff));

        n = read(sockfd, readBuff, sizeof(readBuff)-1);
        size_t bytes_read;
        if(strcmp(readBuff,"FileUpload Accept\n") == 0)
        {
            int b,c;
            printf("Upload Accepted\n");
            verify(UploadName);
            for (b = 0 ; b < 1 ; b++)
            {
                sprintf(cresponse, "%s, ",hdata[b].filename);
                strcat(writeBuff,cresponse);
                for (c = 0 ; c < MD5_DIGEST_LENGTH ; c++)
                {
                    sprintf(cresponse, "%02x",hdata[b].hash[c]);
                    strcat(writeBuff,cresponse);
                }
                sprintf(cresponse, ", %s",ctime(&hdata[b].mtime));
                strcat(writeBuff,cresponse);
            }
                write(sockfd , writeBuff , bytes_read);
                printf("%s\n",writeBuff);
                memset(writeBuff, 0, sizeof(writeBuff));
            fp = fopen(UploadName,"rb");
            while(!feof(fp))
            {
                bytes_read = fread(cresponse, 1, 1024, fp);
                cresponse[1024] = 0;
                memcpy(writeBuff,cresponse,bytes_read);
                write(sockfd , writeBuff , bytes_read);
                memset(writeBuff, 0, sizeof(writeBuff));
                memset(cresponse, 0, sizeof(cresponse));
            }
            memcpy(writeBuff,"~@~",3);
            write(sockfd , writeBuff , 3);
            memset(writeBuff, 0, sizeof(writeBuff));
            memset(readBuff, 0, strlen(readBuff));
            fclose(fp);
        }
        else if(strcmp(readBuff,"FileUpload Deny\n") == 0)
        {
            printf("Upload Denied\n");
            memset(readBuff, 0,sizeof(readBuff));
            continue;
        }
        else
        {
            while(1)
            {
                readBuff[n] = 0;
                if(readBuff[n-1] == '~' && readBuff[n-3] == '~' && readBuff[n-2] == '@')
                {
                    readBuff[n-3] = 0;
                    if(filedownload == 1)
                    {
                        fwrite(readBuff,1,n-3,fp);
                        fclose(fp);
                    }
                    else
                        strcat(cresponse,readBuff);
                    memset(readBuff, 0,strlen(readBuff));
                    break;
                }
                else
                {
                    if(filedownload == 1)
                        fwrite(readBuff,1,n,fp);
                    else
                        strcat(cresponse,readBuff);
                    memset(readBuff, 0,strlen(readBuff));
                }
                n = read(sockfd, readBuff, sizeof(readBuff)-1);
                if(n < 0)
                    break;
            }
        }

        if(filedownload == 0)
            printf("%s\n",cresponse);
        else 
            printf("File Downloaded\n");

        if(n < 0)
            printf("\n Read error \n");
        memset(readBuff, 0,sizeof(readBuff));
        memset(writeBuff, 0,sizeof(writeBuff));
    }
    return 0;
}

int act_as_udp_server(char *listenportno)
{
    int listenfd = 0, connfd = 0;
    struct sockaddr_in serv_addr; 
    int portno = atoi(listenportno);

    char readBuff[1024];
    char writeBuff[1024];
    time_t ticks; 

    listenfd = socket(AF_INET, SOCK_DGRAM, 0);
    memset(&serv_addr, 0, sizeof(serv_addr));
    memset(readBuff, 0, sizeof(readBuff)); 
    memset(writeBuff, 0, sizeof(writeBuff)); 

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(portno); 

    bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)); 


    int a , n , b , c;
    n = read(listenfd,readBuff,sizeof(readBuff));


    while( n > 0)
    {
        size_t size = strlen(readBuff) + 1;
        char *request = malloc(size);
        strcpy(request,readBuff);
    printf("%s\n",request);
    fflush(stdout);

        strcpy(history[hist_count],readBuff);
        hist_count++;

        int type_request = parse_request(request);
        if(type_request == -1)      //Error
        {
            error = 1;
            sprintf(response,"ERROR: No request of this type.\n");
        }
        else if(type_request == 1)      //Indexget
            IndexGet(request);
        else if(type_request == 2)      //FileHash
            FileHash(request);
        else if(type_request == 3)      //FileDownload
            FileDownload(request);

        if(error == 1)
        {
            strcat(writeBuff,response);
            strcat(writeBuff,"~@~");
            write(listenfd , writeBuff , strlen(writeBuff));
            error = -1;
            memset(writeBuff, 0, sizeof(writeBuff));
            memset(readBuff, 0, sizeof(readBuff)); 
            while((n = read(listenfd,readBuff,sizeof(readBuff)))<=0);
            continue;
        }
        if(type_request == 1 && regexpression == 1)
        {
            sprintf(response, "Response %d\n" , i);
            strcat(writeBuff,response);

            for(a = 0 ; a < i ; a++)
            {
                sprintf(response, "%s, %d, %c\n" , pdata[a].filename , pdata[a].size , pdata[a].type );
                strcat(writeBuff,response);
            }
            strcat(writeBuff,"~@~");

            write(listenfd , writeBuff , strlen(writeBuff));
        }
        else if(type_request == 1)
        {
            sprintf(response, "Response %d\n" , i);
            strcat(writeBuff,response);

            for(a = 0 ; a < i ; a++)
            {
                sprintf(response, "%s, %d, %c, %s" , pdata[a].filename , pdata[a].size , pdata[a].type , ctime(&pdata[a].mtime));
                strcat(writeBuff,response);
            }
            strcat(writeBuff,"~@~");

            write(listenfd , writeBuff , strlen(writeBuff));
        }
        else if(type_request == 2)
        {
            sprintf(response, "Response %d\n" , i);
            strcat(writeBuff,response);

            for (b = 0 ; b < i ; b++)
            {
                sprintf(response, "%s, ",hdata[b].filename);
                strcat(writeBuff,response);
                for (c = 0 ; c < MD5_DIGEST_LENGTH ; c++)
                {
                    sprintf(response, "%02x",hdata[b].hash[c]);
                    strcat(writeBuff,response);
                }
                sprintf(response, ", %s",ctime(&hdata[b].mtime));
                strcat(writeBuff,response);
            }
            strcat(writeBuff,"~@~");

            write(listenfd , writeBuff , strlen(writeBuff));
        }
        else if(type_request == 3)
        {
            FILE* fp;
            fp = fopen(fileDownloadName,"rb");
            size_t bytes_read;
            while(!feof(fp))
            {
                bytes_read = fread(response, 1, 1024, fp);
                memcpy(writeBuff,response,bytes_read);
                write(listenfd , writeBuff , bytes_read);
                memset(writeBuff, 0, sizeof(writeBuff));
                memset(response, 0, sizeof(response));
            }
            memcpy(writeBuff,"~@~",3);
            write(listenfd , writeBuff , 3);
            memset(writeBuff, 0, sizeof(writeBuff));
            fclose(fp);
        }
        else if(type_request == 4)
        {
            printf("FileUpload Accepted\n");
            memcpy(writeBuff,"FileUpload Accept\n",18);
            write(listenfd , writeBuff , 18);
            memset(writeBuff, 0,18);
            char copyrequest[1024];
            memset(copyrequest, 0,1024);
            memcpy(copyrequest,request,1024);
            char *size = strtok(copyrequest,"\n");
            size = strtok(NULL,"\n");
            long fsize = atol(size);
            char *request_data = NULL;
            const char delim[] = " \n";
            request_data = strtok(request,delim);
            request_data = strtok(NULL,delim);
            int f;
            int result;
            f = open(request_data, O_WRONLY | O_CREAT | O_EXCL, (mode_t)0600);
            if (f == -1) {
                perror("Error opening file for writing:");
                return 1;
            }
            result = lseek(f,fsize-1, SEEK_SET);
            result = write(f, "", 1);
            if (result < 0) {
                close(f);
                perror("Error opening file for writing:");
                return 1;
            }
            close(f);
            FILE *fp;
            fp = fopen(request_data,"wb");
            n = read(listenfd, readBuff, sizeof(readBuff)-1);
            while(1)
            {
                readBuff[n] = 0;
                if(readBuff[n-1] == '~' && readBuff[n-3] == '~' && readBuff[n-2] == '@')
                {
                    readBuff[n-3] = 0;
                    fwrite(readBuff,1,n-3,fp);
                    fclose(fp);
                    memset(readBuff, 0,n-3);
                    break;
                }
                else
                {
                    fwrite(readBuff,1,n,fp);
                    memset(readBuff, 0,n);
                }
                n = read(listenfd, readBuff, sizeof(readBuff)-1);
                if(n < 0)
                    break;
            }
            memset(writeBuff, 0,1024);

        }

        regexpression = 0;
        memset(readBuff, 0, sizeof(readBuff)); 
        memset(writeBuff, 0, sizeof(writeBuff));
        while((n = read(listenfd,readBuff,sizeof(readBuff)))<=0);
    }
    close(listenfd);
    wait(NULL);
}

int act_as_udp_client(char *ip,char *connectportno)
{
    int sockfd = 0, n = 0;
    char readBuff[1024];
    char writeBuff[1024];
    struct sockaddr_in serv_addr;
    int portno = atoi(connectportno);
    char DownloadName[1024];
    char UploadName[1024];

    memset(readBuff, 0,sizeof(readBuff));
    memset(writeBuff, 0,sizeof(writeBuff));
    if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        printf("\n Error : Could not create socket \n");
        return 1;
    } 

    memset(&serv_addr, 0, sizeof(serv_addr)); 

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(portno); 

    if(inet_pton(AF_INET, ip, &serv_addr.sin_addr)<=0)
    {
        printf("\n inet_pton error occured\n");
        return 1;
    } 

    int a , count = 0 , filedownload = 0 , fileupload = 0;

    while(1)
    {
        filedownload = 0;
        fileupload = 0;
        FILE *fp = NULL;
        int i;
        char *cresponse = malloc(MAX_PACKET_LENGTH);
        fgets(writeBuff,sizeof(writeBuff),stdin);

        char *filename;
        char copy[1024];
        strcpy(copy,writeBuff);
        filename = malloc(1024);
        filename = strtok(copy," \n");
        if(strcmp(filename,"FileDownload") == 0)
        {
            filedownload = 1;
            filename = strtok(NULL," \n");
            strcpy(DownloadName,filename);
            fp = fopen(DownloadName,"wb");
        }
        if(strcmp(filename,"FileUpload") == 0)
        {
            fileupload = 1;
            filename = strtok(NULL," \n");
            strcpy(UploadName,filename);
            FILE *f = fopen(UploadName, "r");
            fseek(f, 0, SEEK_END);
            unsigned long len = (unsigned long)ftell(f);
            char size[1024];
            memset(size, 0, sizeof(size));
            sprintf(size,"%ld\n",len);
            strcat(writeBuff,size);
            fclose(f);
        }
        write(sockfd, writeBuff , strlen(writeBuff));

        n = read(sockfd, readBuff, sizeof(readBuff)-1);
        size_t bytes_read;
        if(strcmp(readBuff,"FileUpload Accept\n") == 0)
        {
            int b,c;
            printf("Response 1\nUpload Accepted\n");
            verify(UploadName);
            for (b = 0 ; b < 1 ; b++)
            {
                sprintf(cresponse, "%s, ",hdata[b].filename);
                strcat(writeBuff,cresponse);
                for (c = 0 ; c < MD5_DIGEST_LENGTH ; c++)
                {
                    sprintf(cresponse, "%02x",hdata[b].hash[c]);
                    strcat(writeBuff,cresponse);
                }
                sprintf(cresponse, ", %s",ctime(&hdata[b].mtime));
                strcat(writeBuff,cresponse);
            }
                write(sockfd , writeBuff , bytes_read);
                printf("%s\n",writeBuff);
                memset(writeBuff, 0, sizeof(writeBuff));
            fp = fopen(UploadName,"rb");
            while(!feof(fp))
            {
                bytes_read = fread(cresponse, 1, 1024, fp);
                cresponse[1024] = 0;
                memcpy(writeBuff,cresponse,bytes_read);
                write(sockfd , writeBuff , bytes_read);
                memset(writeBuff, 0, sizeof(writeBuff));
                memset(cresponse, 0, sizeof(cresponse));
            }
            memcpy(writeBuff,"~@~",3);
            write(sockfd , writeBuff , 3);
            memset(writeBuff, 0, sizeof(writeBuff));
            memset(readBuff, 0, strlen(readBuff));
            fclose(fp);
        }
        else if(strcmp(readBuff,"FileUpload Deny\n") == 0)
        {
            printf("Response 1\nUpload Denied\n");
            memset(readBuff, 0,sizeof(readBuff));
            continue;
        }
        else
        {
            while(1)
            {
                readBuff[n] = 0;
                if(readBuff[n-1] == '~' && readBuff[n-3] == '~' && readBuff[n-2] == '@')
                {
                    readBuff[n-3] = 0;
                    if(filedownload == 1)
                    {
                        fwrite(readBuff,1,n-3,fp);
                        fclose(fp);
                    }
                    else
                        strcat(cresponse,readBuff);
                    memset(readBuff, 0,strlen(readBuff));
                    break;
                }
                else
                {
                    if(filedownload == 1)
                        fwrite(readBuff,1,n,fp);
                    else
                        strcat(cresponse,readBuff);
                    memset(readBuff, 0,strlen(readBuff));
                }
                n = read(sockfd, readBuff, sizeof(readBuff)-1);
                if(n < 0)
                    break;
            }
        }

        if(filedownload == 0)
            printf("%s\n",cresponse);
        else 
            printf("Response 1\nFile Downloaded\n");

        if(n < 0)
            printf("\n Read error \n");
        memset(readBuff, 0,sizeof(readBuff));
        memset(writeBuff, 0,sizeof(writeBuff));
    }
    return 0;
}