//
// Created by paul magos on 18/01/22.
//

#include "../../headers/client.h"


bool pFlag, fFlag, dFlag, DFlag;
long timeToSleep = 0;


int recWrite(char* dirname, char* expelledDir, long cnt, int indent);
char* charToString(char str);

void Helper(){
    fprintf(stdout,
        "-h \t\t\t help (commands description)\n"
               "-f filename  \t\t socket name (AF_UNIX socket)\n"
               "-w dirname[,n=0] \t sends n files from dirname, if n=0 it sends all the files it can;"
               "recursively access sub directories\n"
               "-W file1[,file2] \t list of files to send to the server, detached by comma\n"
               "-D dirname \t\t expelled files (from server) directory for client, it has to be used "
               "with -w or -W, if not specified the files sent by the server will be ignored by the client\n"
               "-r file1[,file2] \t list of files to read from the server, detached by comma\n"
               "-R [n=0] \t\t read n files from the server, if n = 0 or not specified, it reads all "
               "the files on the server\n"
               "-d dirname \t\t directory name for storing the files readen from the server\n"
               "-t time \t\t waiting time between queries to the server\n"
               "-l file1[,file2] \t list of file on which we need mutual exclusion\n"
               "-u file1[,file2] \t list of file on which we have to release mutual exclusion\n"
               "-c file1[,file2] \t list of file we want to remove from the server\n"
               "-p \t\t\t enables print on the standard output for each operation \n"
            );
}

int getCmdList(List* opList, int argc, char* argv[]){
    pFlag = false;
    fFlag = false;
    bool hFlag = false;
    char option;
    int test;
    char* timeArg = NULL;
    char* fArg = NULL;

    while((option = (char)getopt(argc, argv, "hpf:w:W:D:r:R::d:t:l:u:c:")) != -1){
        if(optarg) {
            if(optarg[0]=='-'){
                fprintf(stderr, "%c command needs at least one parameter\n", option);
                errno = EINVAL;
                return -1;
            }
        }
        switch (option) {
            case 'p': {
                if(!pFlag){
                    fprintf(stdout, "SUCCESS - standard output activated\n");
                    (pFlag = true);
                } else{
                    fprintf(stderr, "ERROR - standard output already required, require it once\n");
                    errno = EINVAL;
                    return -1;
                }
                break;
            }
            case 'f': {
                if((fFlag)){
                    fprintf(stderr, "ERROR - socket can be set only once\n");
                    errno = EINVAL;
                    return -1;
                }else {
                    SYSCALL_EXIT(pushBottom, test, pushBottom(&(*opList), charToString(option),optarg),
                                 "Error List Push, errno = %d\n", errno);
                    fArg = optarg;
                    ((fFlag) = true);
                    break;
                }
            }
            case 'h': {
                hFlag = true;
                break;
            }
            case 't': {
                if(timeArg==NULL) timeArg = optarg;
                break;
            }
            case ':': {
                fprintf(stderr, "%c command needs at least one parameter\n", optopt);
                errno = EINVAL;
                return -1;
            }
            case '?': {
                fprintf(stderr, "Run with -h to see command list\n");
                errno = EINVAL;
                return -1;
            }
            default:{
                SYSCALL_EXIT(pushBottom, test, pushBottom(&(*opList), charToString(option),optarg),
                             "Error List Push, errno = %d\n", errno);
                break;
            };
        }

    }
    if(hFlag) Helper();
    if(timeArg!=NULL){
        SYSCALL_EXIT(StringToLong, timeToSleep, StringToLong(timeArg),
                     "ERROR - Time Char '%s' to Long Conversion gone wrong, errno=%d\n", timeArg, errno);
        if(pFlag) fprintf(stdout, "SUCCESS - time = %lu\n", timeToSleep);
    }
    if(fFlag){
        // Provo a collegarmi per 10 secondi
        struct timespec absTime = {10, 0};
        SYSCALL_EXIT(openConnection,
                     test,
                     openConnection(fArg,
                                    1000,
                                    absTime),
                     "Connection error to socket %s, errno %d\n",
                     fArg,
                     errno);
        if(pFlag) fprintf(stdout, "SUCCESS - Connected to %s\n", fArg);
        msleep(timeToSleep);
    }
    if(search((*opList)->head, "D")){
        if(!(search((*opList)->head, "w") || search((*opList)->head, "W"))) {
            fprintf(stderr, "ERROR - D command must be used with 'w' or 'W' commands\n");
            errno = EINVAL;
            return -1;
        }
        DFlag = true;
    } else DFlag = false;
    if(search((*opList)->head, "d")){
        if(!(search((*opList)->head, "r") || search((*opList)->head, "R"))) {
            fprintf(stderr, "ERROR - d command must be used with 'r' or 'R' commands\n");
            errno = EINVAL;
            return -1;
        }
        dFlag = true;
    } else dFlag = false;
    commandHandler(&(*opList));
    return 0;
}

void commandHandler(List* commandList){
    // Arguments for f, d, D, w, R
    char* socket = NULL;
    char* readDir = NULL;
    char* expelledDir = NULL;
    long numOfFilesToWrite = INT_MAX;
    long numOfFilesToRead = INT_MAX;

    // Response for SYSCALL_EXIT
    int scRes;

    // Command and argument for each element of opList
    char* command = NULL;
    void* argument = NULL;

    // Path details, to establish if it's a directory
    struct stat dir_Details;
    char* token; // For strtok_r
    char* rest; // For strtok_r
    char* path = NULL;
    char* temporary;

    // Control if given arguments for expelled files, and readen files from server are directories
    if(DFlag) {
        getArg((*commandList)->head, "D", &expelledDir);
        if(expelledDir) {
            stat(expelledDir,&dir_Details);
            if(!S_ISDIR(dir_Details.st_mode))  expelledDir = "dev/null";
            else if(pFlag) fprintf(stdout, "SUCCESS - Expelled Directory set : %s\n", expelledDir);
        }
        memset(&dir_Details, 0, sizeof(dir_Details));
    }
    if(dFlag) {
        getArg((*commandList)->head, "d", &readDir);
        if (readDir) {
            stat(readDir, &dir_Details);
            if (!S_ISDIR(dir_Details.st_mode)) readDir = "dev/null";
            else if (pFlag) fprintf(stdout, "SUCCESS - Received Files Directory set : %s\n", readDir);
        }
        memset(&dir_Details, 0, sizeof(dir_Details));
    }

    while ( pullTop(&(*commandList), &command, &argument) == 0){
        switch (*command) {
            case 'w':{
                token = strtok_r(argument, ",", &rest);
                stat(token, &dir_Details);
                if(S_ISDIR(dir_Details.st_mode)){
                    if((temporary = strtok_r(NULL, ",", &rest)) != NULL)
                        SYSCALL_EXIT(StringToLong, numOfFilesToWrite, StringToLong(temporary),
                                     "Char '%s' to Long Conversion gone wrong, errno=%d\n", temporary, errno);
                    if(pFlag) fprintf(stdout, "Accessing Folder %s : \n", token);
                    recWrite(token, expelledDir, numOfFilesToWrite, 0);
                } else{
                    fprintf(stderr, "ERROR - writing files from directory '%s', not a valid directory\n",token);
                }
                memset(&dir_Details, 0, sizeof(dir_Details));
                msleep(timeToSleep);
                break;
            }
            case 'W':{
                token = strtok_r(argument, ",", &rest);
                stat(token, &dir_Details);
                path = NULL;
                if((path = realpath(token, path)) == NULL) fprintf(stderr, "ERROR - Opening File %s\n", token);
                else{
                    SYSCALL_EXITFREE(openFile, path, scRes, openFile(path, O_CREAT),
                        "ERROR - Couldn't send file %s to server, errno = %d\n", token, errno);
                    SYSCALL_EXITFREE(writeFile, path, scRes, writeFile(path, expelledDir),
                        "ERROR - Couldn't write file %s on server, errno = %d\n", token, errno);
                    SYSCALL_EXITFREE(closeFile, path, scRes, closeFile(path),
                        "ERROR - Couldn't close file %s on server, errno = %d\n", token, errno);
                    if(pFlag) fprintf(stdout,"%s -> WRITE SUCCESS\n", token);
                }
                free(path);
                msleep(timeToSleep);
                break;
            }
            case 'r':{
                token = strtok_r(argument, ",", &rest);
                stat(token, &dir_Details);
                path = NULL;
                while (token) {
                    if ((path = realpath(token, path)) == NULL) fprintf(stderr, "ERROR - Opening File %s\n", token);
                    else {
                        SYSCALL_EXITFREE(openFile, path, scRes, openFile(path, 0), (pFlag) ?
                            "ERROR - Couldn't get file %s to server, errno = %d\n" : "", token, errno);
                        char *buffer;
                        size_t size;
                        SYSCALL_EXITFREE(readFile, path, scRes, readFile(path, (void **) &buffer, &size), (pFlag) ?
                            "ERROR - Couldn't write file %s on server, errno = %d\n" : "", token, errno);
                        if (readDir != NULL){
                            char clientPath[UNIX_PATH_MAX];
                            memset(clientPath, 0, UNIX_PATH_MAX);
                            char* fileName = token;
                            sprintf(clientPath, "%s/%s", readDir, fileName);

                            FILE* clientFile = fopen(clientPath, "w");
                            if(clientFile){ fprintf(clientFile, "%s", buffer);}
                            else fprintf(stderr, "ERROR - Saving file %s", token);
                            fclose(clientFile);
                        }

                        SYSCALL_EXITFREE(closeFile, path, scRes, closeFile(path), (pFlag) ?
                            "ERROR - Couldn't close file %s on server, errno = %d\n" : "", token, errno);
                        if (pFlag) fprintf(stdout, "%s -> WRITE SUCCESS\n", token);
                    }
                    token = strtok_r(NULL, ",", &rest);
                    msleep(timeToSleep);
                }
                free(path);
                break;
            }
            case 'R':{
                if(argument!=NULL)
                    SYSCALL_EXIT(StringToLong, numOfFilesToRead, StringToLong(argument),
                                 "ERROR - ReadNF Char '%s' to Long Conversion gone wrong, errno=%d\n", argument, errno);
                SYSCALL_EXIT(readNFiles, scRes, readNFiles(numOfFilesToRead, readDir),
                             "ERROR - ReadNF lettura file, errno = %d\n", errno);
                if(pFlag) fprintf(stdout, "SUCCESS - Lettura di file\n");
                msleep(timeToSleep);
                break;
            }
            case 'l':{
                token = strtok_r(argument, ",", &rest);
                path = NULL;
                while (token){
                    if(!(path = realpath(token, path))){
                        fprintf(stderr, "ERROR - file '%s' not exits", token);
                    } else{
                        stat(path, &dir_Details);
                        if(S_ISREG(dir_Details.st_mode)){
                            SYSCALL_EXITFREE(openFile, path, scRes, openFile(path, 0), (pFlag) ?
                                         "ERROR - Couldn't open file %s from server, errno = %d\n" : "", token, errno);
                            SYSCALL_EXITFREE(lockFile, path, scRes, lockFile(path),
                                         "ERROR - lock file %s, errno = %d", token, errno);
                            SYSCALL_EXITFREE(closeFile, path, scRes, closeFile(path), (pFlag) ?
                                        "ERROR - Couldn't close file %s on server, errno = %d\n" : "", token, errno);
                            if(pFlag) fprintf(stdout, "LockFile: File %s Success", token);
                        }else{
                            fprintf(stderr, "ERROR - '%s' not a file", token);
                        }
                    }
                    token = strtok_r(NULL, ",", &rest);
                    msleep(timeToSleep);
                }
                free(path);
                memset(&dir_Details, 0, sizeof(dir_Details));
                break;
            }
            case 'u':{
                token = strtok_r(argument, ",", &rest);
                path = NULL;
                while (token){
                    if(!(path = realpath(token, path))){
                        fprintf(stderr, "ERROR - file '%s' not exits", token);
                    } else{
                        stat(path, &dir_Details);
                        if(S_ISREG(dir_Details.st_mode)){
                            SYSCALL_EXITFREE(openFile, path, scRes, openFile(path, 0), (pFlag) ?
                                         "ERROR - Couldn't open file %s from server, errno = %d\n" : "", token, errno);
                            SYSCALL_EXITFREE(unlockFile, path, scRes, unlockFile(path),
                                         "ERROR - Unlock file %s, errno = %d", token, errno);
                            SYSCALL_EXITFREE(closeFile, path, scRes, closeFile(path), (pFlag) ?
                                        "ERROR - Couldn't close file %s on server, errno = %d\n" : "", token, errno);
                            if(pFlag) fprintf(stdout, "SUCCESS - %s file unlocked", token);
                        }else{
                            fprintf(stderr, "ERROR - '%s' not a file", token);
                        }
                    }
                    token = strtok_r(NULL, ",", &rest);
                    msleep(timeToSleep);
                }
                free(path);
                memset(&dir_Details, 0, sizeof(dir_Details));
                break;
            }
            case 'c':{
                token = strtok_r(argument, ",", &rest);
                path = NULL;
                while (token){
                    if(!(path = realpath(token, path))){
                        fprintf(stderr, "ERROR - file '%s' not exits", token);
                    } else{
                        stat(path, &dir_Details);
                        if(S_ISREG(dir_Details.st_mode)){
                            SYSCALL_EXITFREE(removeFile, path, scRes, removeFile(path),
                                         "ERROR - Remove file %s, errno = %d", token, errno);
                            if(pFlag) fprintf(stdout, "SUCCESS - %s file removed", token);
                        }else{
                            fprintf(stderr, "ERROR - '%s' not a file", token);
                        }
                    }
                    token = strtok_r(NULL, ",", &rest);
                    msleep(timeToSleep);
                }
                free(path);
                memset(&dir_Details, 0, sizeof(dir_Details));
                break;
            }
            case 'D':{
                break;
            }
            case 'd':{
                break;
            }
            case 'f':{
                socket = argument;
                break;
            }
        }
    }
    msleep(timeToSleep);
    SYSCALL_EXIT(closeConnection, scRes, closeConnection(socket), "ERROR closing connection to %s, errno = %d", socket, errno);
    return;
}

int recWrite(char* dirname, char* expelledDir, long cnt, int indent){
    DIR* directory;
    struct dirent* element;
    long filesToWrite = cnt;
    if(dirname[sizeof(dirname)-1] == '/') dirname[sizeof(dirname)-1] = '\0';
    int scRes;
    if((directory = opendir(dirname))==NULL || filesToWrite == 0) return 0;

    errno = 0;
    // StackOverflow has something similar at
    // https://stackoverflow.com/questions/8436841/how-to-recursively-list-directories-in-c-on-linux/29402705
    while ((element = readdir(directory)) != NULL && filesToWrite > 0){
        char newPath[PATH_MAX];
        snprintf(newPath, sizeof(newPath), "%s/%s", dirname, element->d_name);
        switch(element->d_type){
            case DT_REG:{
                char* path = NULL;
                if((path = realpath(newPath, path)) == NULL) fprintf(stderr, "ERROR - Opening File %s\n", newPath);
                else{
                    SYSCALL_EXIT(openFile, scRes, openFile(path, 1), (pFlag)?
                        "ERROR - Couldn't send file %s to server, errno = %d\n": "", element->d_name, errno);
                    SYSCALL_EXIT(writeFile, scRes, writeFile(path, expelledDir), (pFlag)?
                        "ERROR - Couldn't write file %s on server, errno = %d\n": "", element->d_name, errno);
                    SYSCALL_EXIT(closeFile, scRes, closeFile(path), (pFlag)?
                        "ERROR - Couldn't close file %s on server, errno = %d\n": "", element->d_name, errno);
                    if(pFlag) printf("%*s- %s -> WRITE SUCCESS\n", indent, "", element->d_name);
                    filesToWrite--;
                }
                free(path);
                msleep(timeToSleep);
                break;
            }
            case DT_DIR: {
                if (strcmp(element->d_name, ".") == 0 || strcmp(element->d_name, "..") == 0) continue;
                if (pFlag) printf("%*s[%s]\n", indent, "", element->d_name);
                filesToWrite  = filesToWrite - recWrite(newPath, expelledDir, filesToWrite, indent + 2);
                break;
            }
            default:{
                break;
            }
        }
    }
    closedir(directory);
    return cnt - filesToWrite;
}

char* charToString(char str){
    switch (str) {
        case 'f': {
            return "f";
        }
        case 'w': {
            return "w";
        }
        case 'W': {
            return "W";
        }
        case 'D': {
            return "D";
        }
        case 'r': {
            return "r";
        }
        case 'R': {
            return "R";
        }
        case 'd': {
            return "d";
        }
        case 'l': {
            return "l";
        }
        case 'c': {
            return "c";
        }
        case 'u': {
            return "u";
        }
    }
    return 0;
}




