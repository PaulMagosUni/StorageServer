//
// Created by paul on 26/01/22.
//

#include "../../../headers/server.h"


int opExecute(int fd, int workerId, message* message1);
int lastOpUpdate(serverFile* file, fileFlags op, int thread);
int ExpelledHandler(int fd, int workerId, List expelled);


void taskExecute(void* argument){
    if( argument == NULL ) {
        fprintf(stderr, "ERROR - Invalid Worker Argument, errno = %d", EINVAL);
        exit(EXIT_FAILURE);
    }

    wTask *tmpArgs = argument;
    int myId = tmpArgs->worker_id;
    int fd_client = (int)tmpArgs->client_fd;
    int pipeM = tmpArgs->pipeT;
    free(argument);
    /*int r = -1;
    if(writen(pipeM, &r, sizeof(int))==-1){
        fprintf(stderr, "ERROR - Thread %d: writing on pipe\n", myId);
    }*/


    appendOnLog(ServerLog, "[Thread %d]: Client %d Task\n", myId, fd_client);
    if(ServerStorage->stdOutput) printf("[Thread %d]: Client %d Task\n", myId, fd_client);

    message message1;
    memset(&message1, 0, sizeof(message));

    if(readMessage(fd_client, &message1) == -1){
        appendOnLog(ServerLog, "[Thread %d]: Client %d, connection closed\n", myId, fd_client);
        if(ServerStorage->stdOutput) printf("[Thread %d]: Client %d connection closed\n", myId, fd_client);
        if(CloseConnection((int)fd_client, (int)myId) == -1) {
            fprintf(stderr, "[Thread %d]: ERROR - Closing connection to client fault, errno = %d", myId, errno);
        }
        freeMessageContent(&message1);
        return;
    }
    if(opExecute(fd_client, myId, &message1)==0){
        if(writeMessage(fd_client, &message1)==-1){
            if(ServerStorage->stdOutput)
                printf("[Thread %d]: FATAL ERROR, writing message to client %d\n", myId, fd_client);
            appendOnLog(ServerLog, "[Thread %d]: FATAL ERROR, writing %s to client %d\n", myId, (message1.size>0)? message1.content: requestToString(message1.request),fd_client);
            if(message1.size>0) freeMessageContent(&message1);
            return;
        }else {
            if(message1.feedback==ERROR){
                if(ServerStorage->stdOutput)
                    printf("[Thread %d]: Client %d request %s %s failed - %s\n", myId, fd_client, (message1.size>0)?(char*)message1.content:"",
                            requestToString(message1.request), strerror(message1.additional));
                appendOnLog(ServerLog, "[Thread %d]: Client %d request %s %s failed - %s\n", myId, fd_client, (message1.size>0)?(char*)message1.content:"",
                            requestToString(message1.request), strerror(message1.additional));
                freeMessageContent(&message1);
            }
            if (writen(pipeM, &fd_client, sizeof(int)) == -1) {
                if(ServerStorage->stdOutput) fprintf(stderr, "ERROR - Thread %d: writing on pipe", myId);
            }
        }
    }
    if(message1.size>0) freeMessageContent(&message1);
    return;
}

int opExecute(int fd, int workerId, message* message1){
    switch (message1->request) {
        case O_WRITE:{
            ReceiveFile(fd, workerId, message1);
            break;
        }   // write
        case O_READ:{
            SendFile(fd, workerId, message1);
            break;
        }   // read file
        case O_READN:{
            SendNFiles(fd, workerId, message1);
            break;
        }   // read nFiles
        case (O_LOCK):{
            LockFile(fd, workerId, message1);
            break;
        }   // lock file
        case O_UNLOCK:{
            UnLockFile(fd, workerId, message1);
            break;
        }   // unlock file
        case O_DEL:{
            DeleteFile(fd, workerId, message1);
            break;
        }   // delete file
        case O_CLOSE:{
            CloseFile(fd, workerId, message1);
            break;
        }   // close file
        case O_APPND:{
            AppendOnFile(fd, workerId, message1);
            break;
        }   // append to file
        case O_CREAT:
        case O_CREAT_LOCK:
        case O_PEN:{
            OpenFile(fd, workerId, message1);
            break;
        }   // openFile
        default:{
            message1->additional = EBADMSG;
            message1->feedback = ERROR;
            return -1;
        }
    }
    return 0;
}

int CloseConnection(int fd_client, int workerId){
    if(fd_client == -1 || workerId == -1){
        errno = EINVAL;
        return -1;
    }
    SYSCALL_RETURN(closeConn_pthread_mutex_lock, pthread_mutex_lock(&(ServerStorage->lock)),
                   "Error - Server Lock failure, errno = %d", errno);
    if(ServerStorage->stdOutput) printf("[Thread %d]: Server Lock\n", workerId);


    if(ServerStorage->actualFilesNumber>0) {
        serverFile *file;
        icl_entry_t *bucket, *curr;
        for (int i = 0; i < ServerStorage->filesTable->nbuckets; i++) {
            bucket = ServerStorage->filesTable->buckets[i];
            for (curr = bucket; curr != NULL; curr = curr->next) {
                file = ((serverFile *) curr->data);
                if (file->writers != 0) {
                    continue;
                }
                fileWritersIncrement(file, workerId);
                if (searchInt(file->clients_fd->head, fd_client)==1) {
                    removeByInt(&file->clients_fd, &fd_client);
                    if (file->lockFd == fd_client) file->lockFd = -1;
                }
                fileWritersDecrement(file, workerId);
            }
        }
    }
    SYSCALL_RETURN(closeConn_pthread_mutex_lock, pthread_mutex_unlock(&(ServerStorage->lock)),
                   "Error - Server unLock failure, errno = %d", errno);
    if(ServerStorage->stdOutput) printf("[Thread %d]: Server UnLock\n", workerId);


    close(fd_client);
    return 0;
}

void OpenFile(int fd_client, int workerId, message* message1){
    if(fd_client == -1 || workerId == -1) {
        errno = EINVAL;
        message1->additional = errno;
        message1->feedback = ERROR;
        return;
    }

    SYSCALL_RET(pthred_lock, pthread_mutex_lock(&(ServerStorage->lock)),
                   "ERROR - ServerStorage lock acquisition failure, errno = %d", errno)
    if(ServerStorage->stdOutput) printf("[Thread %d]: Server Lock\n", workerId);


    serverFile* File = icl_hash_find(ServerStorage->filesTable, message1->content);

    switch (message1->request) {
        case O_CREAT:{
            if(File!=NULL) {
                SYSCALL_RET(pthred_unlock, pthread_mutex_unlock(&(ServerStorage->lock)),
                               "ERROR - ServerStorage lock release failure, errno = %d", errno)
                if(ServerStorage->stdOutput) printf("[Thread %d]: Server UnLock\n", workerId);

                errno = EEXIST;
                message1->feedback = ERROR;
                message1->additional = errno;
                return;
            }
            else{
                if(ServerStorage->actualFilesNumber == ServerConfig.maxFile){
                    serverFile *expelledFile     = icl_hash_toReplace(ServerStorage->filesTable, ServerConfig.policy, workerId);
                    if(expelledFile==NULL) {
                        if(message1->size>0) freeMessageContent(message1);
                        return;
                    }
                    fileWritersIncrement(expelledFile, workerId);
                    expelledFile->toDelete = fd_client;
                    ServerStorage->expelledBytes+=expelledFile->size;
                    ServerStorage->expelledFiles++;
                    appendOnLog(ServerLog, "[Thread %d]: File %s deleted for new %s file by client %d\n", workerId, expelledFile->path, message1->content, fd_client);
                    if(ServerStorage->stdOutput) printf("[Thread %d]: File %s deleted for new %s file by client %d\n", workerId, expelledFile->path, (char*)message1->content, fd_client);
                    fileWritersDecrement(expelledFile, workerId);
                    fileBytesDecrement(expelledFile->size);
                    fileNumDecrement();
                }
                File = malloc(sizeof(serverFile));
                if(!File) {
                    message1->feedback = ERROR;
                    message1->additional = ENOMEM;
                    SYSCALL_RET(pthred_unlock, pthread_mutex_unlock(&(ServerStorage->lock)),
                                   "ERROR - ServerStorage lock release failure, errno = %d", errno)
                    if(ServerStorage->stdOutput) printf("[Thread %d]: Server UnLock\n", workerId);

                    return;
                }
                if(clock_gettime(CLOCK_REALTIME, &(File->creationTime))==-1){
                    free(File);
                    SYSCALL_RET(pthred_unlock, pthread_mutex_unlock(&(ServerStorage->lock)),
                                   "ERROR - ServerStorage lock release failure, errno = %d", errno)
                    if(ServerStorage->stdOutput) printf("[Thread %d]: Server UnLock\n", workerId);

                    return;
                }
                if(clock_gettime(CLOCK_REALTIME, &(File->lastOpTime))==-1){
                    free(File);
                    SYSCALL_RET(pthred_unlock, pthread_mutex_unlock(&(ServerStorage->lock)),
                                   "ERROR - ServerStorage lock release failure, errno = %d", errno)
                    if(ServerStorage->stdOutput) printf("[Thread %d]: Server UnLock\n", workerId);

                    return;
                }
                File->path = malloc(message1->size+1);
                if(!(File->path)){
                    freeMessage(message1);
                    free(File);
                    SYSCALL_RET(pthred_unlock, pthread_mutex_unlock(&(ServerStorage->lock)),
                                   "ERROR - ServerStorage lock release failure, errno = %d", errno)
                    if(ServerStorage->stdOutput) printf("[Thread %d]: Server UnLock\n", workerId);

                    return;
                }
                strncpy(File->path, message1->content, message1->size+1);
                createList(&File->clients_fd);
                pushTop(&File->clients_fd, NULL, &fd_client);
                if(pthread_mutex_init(&(File->lock), NULL) == -1){
                    free(File->path);
                    free(File);
                    SYSCALL_RET(pthred_unlock, pthread_mutex_unlock(&(ServerStorage->lock)),
                                   "ERROR - ServerStorage lock release failure, errno = %d", errno)
                    if(ServerStorage->stdOutput) printf("[Thread %d]: Server UnLock\n", workerId);

                    return;
                }
                if(pthread_mutex_init(&(File->order), NULL) == -1){
                    free(File->path);
                    pthread_mutex_destroy(&(File->lock));
                    free(File);
                    SYSCALL_RET(pthred_unlock, pthread_mutex_unlock(&(ServerStorage->lock)),
                                   "ERROR - ServerStorage lock release failure, errno = %d", errno)
                    if(ServerStorage->stdOutput) printf("[Thread %d]: Server UnLock\n", workerId);

                    return;
                }
                if(pthread_cond_init(&(File->condition), NULL) == -1){
                    free(File->path);
                    pthread_mutex_destroy(&(File->lock));
                    pthread_mutex_destroy(&(File->order));
                    free(File);
                    SYSCALL_RET(pthred_unlock, pthread_mutex_unlock(&(ServerStorage->lock)),
                                   "ERROR - ServerStorage lock release failure, errno = %d", errno)
                    if(ServerStorage->stdOutput) printf("[Thread %d]: Server UnLock\n", workerId);

                    return;
                }
                File->size = 0;
                File->writers = 0;
                File->readers = 0;
                File->toDelete = 0;
                File->content = NULL;
                File->latsOp = O_CREAT;
                File->lockFd = fd_client;
                if(icl_hash_insert(ServerStorage->filesTable, File->path, File) == NULL){
                    fprintf(stderr, "ERROR - Insert file %s failure", File->path);
                    freeFile(File);
                    SYSCALL_RET(pthred_unlock, pthread_mutex_unlock(&(ServerStorage->lock)),
                                   "ERROR - ServerStorage lock release failure, errno = %d", errno)
                    if(ServerStorage->stdOutput) printf("[Thread %d]: Server UnLock\n", workerId);

                    return;
                }

                fileNumIncrement();
                SYSCALL_RET(pthred_unlock, pthread_mutex_unlock(&(ServerStorage->lock)),
                               "ERROR - ServerStorage lock release failure, errno = %d", errno);
                if(ServerStorage->stdOutput) printf("[Thread %d]: Server UnLock\n", workerId);

                message1->feedback=SUCCESS;
                if(ServerStorage->stdOutput) printf("[Thread %d]: File %s created for client %d\n", workerId, File->path, fd_client);
                appendOnLog(ServerLog, "[Thread %d]: File %s created for client %d\n", workerId, File->path, fd_client);
                return;
            }
            break;
        }
        case O_LOCK:{
            if(File!=NULL){
                if(isLocked(File, fd_client) == 0){
                    SYSCALL_RET(pthred_unlock, pthread_mutex_unlock(&(ServerStorage->lock)),
                                   "ERROR - ServerStorage lock release failure, errno = %d", errno)
                    if(ServerStorage->stdOutput) printf("[Thread %d]: Server UnLock\n", workerId);

                    errno = EDEADLK;
                    message1->additional = errno;
                    message1->feedback = ERROR;
                    return;
                }
                if(isLocked(File, fd_client) == 1){
                    SYSCALL_RET(pthred_unlock, pthread_mutex_unlock(&(ServerStorage->lock)),
                                   "ERROR - ServerStorage lock release failure, errno = %d", errno)
                    if(ServerStorage->stdOutput) printf("[Thread %d]: Server UnLock\n", workerId);

                    errno = EBUSY;
                    message1->additional = errno;
                    message1->feedback = ERROR;
                    return;
                }
                fileWritersIncrement(File, workerId);

                if(searchInt(File->clients_fd->head, fd_client) == 0){
                    pushTop(&File->clients_fd, NULL, &fd_client);
                } else{
                    SYSCALL_RET(pthred_unlock, pthread_mutex_unlock(&(ServerStorage->lock)),
                                   "ERROR - ServerStorage lock release failure, errno = %d", errno)
                    if(ServerStorage->stdOutput) printf("[Thread %d]: Server UnLock\n", workerId);

                    return;
                }
                File->lockFd = fd_client;
                SYSCALL_RET(pthred_unlock, pthread_mutex_unlock(&(ServerStorage->lock)),
                               "ERROR - ServerStorage lock release failure, errno = %d", errno)
                if(ServerStorage->stdOutput) printf("[Thread %d]: Server UnLock\n", workerId);

                fileWritersDecrement(File, workerId);
                lastOpUpdate(File, O_LOCK, workerId);
                message1->feedback = SUCCESS;
                if(ServerStorage->stdOutput) printf("[Thread %d]: File %s locked by client %d\n", workerId, File->path, fd_client);
                appendOnLog(ServerLog,"[Thread %d]: File %s locked by client %d\n", workerId, File->path, fd_client);
                return;
            } else{
                SYSCALL_RET(pthred_unlock, pthread_mutex_unlock(&(ServerStorage->lock)),
                               "ERROR - ServerStorage lock release failure, errno = %d", errno)

                if(ServerStorage->stdOutput) printf("[Thread %d]: Server UnLock\n", workerId);

                errno = ENOENT;
                message1->additional = errno;
                message1->feedback = ERROR;
                return;
            }
        }
        case (O_CREAT_LOCK):{
            if(File==NULL) {
                if(ServerStorage->actualFilesNumber==ServerConfig.maxFile){
                    serverFile* expelled = icl_hash_toReplace(ServerStorage->filesTable, ServerConfig.policy, workerId);
                    fileWritersIncrement(expelled, workerId);
                    expelled->toDelete = fd_client;
                    ServerStorage->expelledBytes+=expelled->size;
                    ServerStorage->expelledFiles++;
                    appendOnLog(ServerLog, "[Thread %d]: File %s deleted for new %s file by client %d\n", workerId, expelled->path, message1->content, fd_client);
                    if(ServerStorage->stdOutput) printf("[Thread %d]: File %s deleted for new %s file by client %d\n", workerId, expelled->path, (char*)message1->content, fd_client);
                    fileWritersDecrement(expelled, workerId);
                    fileBytesDecrement(expelled->size);
                    fileNumDecrement();
                }
                File = malloc(sizeof(serverFile));
                if(!File) {
                    SYSCALL_RET(pthred_unlock, pthread_mutex_unlock(&(ServerStorage->lock)),
                                   "ERROR - ServerStorage lock release failure, errno = %d", errno)
                    if(ServerStorage->stdOutput) printf("[Thread %d]: Server UnLock\n", workerId);

                    return;
                }
                if(clock_gettime(CLOCK_REALTIME, &(File->creationTime))==-1){
                    free(File);
                    SYSCALL_RET(pthred_unlock, pthread_mutex_unlock(&(ServerStorage->lock)),
                                   "ERROR - ServerStorage lock release failure, errno = %d", errno)
                    if(ServerStorage->stdOutput) printf("[Thread %d]: Server UnLock\n", workerId);

                    return;
                }
                if(clock_gettime(CLOCK_REALTIME, &(File->lastOpTime))==-1){
                    free(File);
                    SYSCALL_RET(pthred_unlock, pthread_mutex_unlock(&(ServerStorage->lock)),
                                   "ERROR - ServerStorage lock release failure, errno = %d", errno)
                    if(ServerStorage->stdOutput) printf("[Thread %d]: Server UnLock\n", workerId);

                    return;
                }
                File->path = malloc(message1->size);
                if(!(File->path)){
                    free(File);
                    SYSCALL_RET(pthred_unlock, pthread_mutex_unlock(&(ServerStorage->lock)),
                                   "ERROR - ServerStorage lock release failure, errno = %d", errno)
                    if(ServerStorage->stdOutput) printf("[Thread %d]: Server UnLock\n", workerId);

                    return;
                }
                strncpy(File->path, message1->content, message1->size);
                freeMessageContent(message1);
                createList(&File->clients_fd);
                if(pthread_mutex_init(&(File->lock), NULL) == -1){
                    free(File->path);
                    free(File);
                    SYSCALL_RET(pthred_unlock, pthread_mutex_unlock(&(ServerStorage->lock)),
                                   "ERROR - ServerStorage lock release failure, errno = %d", errno)
                    if(ServerStorage->stdOutput) printf("[Thread %d]: Server UnLock\n", workerId);

                    return;
                }
                if(pthread_mutex_init(&(File->order), NULL) == -1){
                    free(File->path);
                    pthread_mutex_destroy(&(File->lock));
                    free(File);
                    SYSCALL_RET(pthred_unlock, pthread_mutex_unlock(&(ServerStorage->lock)),
                                   "ERROR - ServerStorage lock release failure, errno = %d", errno)
                    if(ServerStorage->stdOutput) printf("[Thread %d]: Server UnLock\n", workerId);

                    return;
                }
                if(pthread_cond_init(&(File->condition), NULL) == -1){
                    free(File->path);
                    pthread_mutex_destroy(&(File->lock));
                    pthread_mutex_destroy(&(File->order));
                    free(File);
                    SYSCALL_RET(pthred_unlock, pthread_mutex_unlock(&(ServerStorage->lock)),
                                   "ERROR - ServerStorage lock release failure, errno = %d", errno);
                    if(ServerStorage->stdOutput) printf("[Thread %d]: Server UnLock\n", workerId);

                    return;
                }
                if(icl_hash_insert(ServerStorage->filesTable, File->path, File) == NULL){
                    fprintf(stderr, "ERROR - Insert file %s failure", File->path);
                    freeFile(File);
                    SYSCALL_RET(pthred_unlock, pthread_mutex_unlock(&(ServerStorage->lock)),
                                   "ERROR - ServerStorage lock release failure, errno = %d", errno)
                    if(ServerStorage->stdOutput) printf("[Thread %d]: Server UnLock\n", workerId);

                    return;
                }
                File->size = 0;
                File->writers = 0;
                File->readers = 0;
                File->lockFd = -1;
                File->toDelete = 0;
                File->content = NULL;
                File->latsOp = O_CREAT;
                fileNumIncrement();
                if(ServerStorage->stdOutput) printf("[Thread %d]: File %s created by client %d\n", workerId, File->path, fd_client);
                appendOnLog(ServerLog,"[Thread %d]: File %s created by client %d\n", workerId, File->path, fd_client);
            }
            if(isLocked(File, fd_client) == -1){
                SYSCALL_RET(pthred_unlock, pthread_mutex_unlock(&(ServerStorage->lock)),
                               "ERROR - ServerStorage lock release failure, errno = %d", errno)
                if(ServerStorage->stdOutput) printf("[Thread %d]: Server UnLock\n", workerId);

                errno = EDEADLK;
                message1->additional = errno;
                message1->feedback = ERROR;
                return;
            }
            if(isLocked(File, fd_client) == 1){
                SYSCALL_RET(pthred_unlock, pthread_mutex_unlock(&(ServerStorage->lock)),
                               "ERROR - ServerStorage lock release failure, errno = %d", errno)
                if(ServerStorage->stdOutput) printf("[Thread %d]: Server UnLock\n", workerId);

                errno = EBUSY;
                message1->additional = errno;
                message1->feedback = ERROR;
                return;
            }
            fileWritersIncrement(File, workerId);
            SYSCALL_RET(pthread_lock, pthread_mutex_lock(&(File->lock)),
                           "ERROR - Lock file %s failure", File->path)
            if( searchInt(File->clients_fd->head, fd_client) == 0)
                pushTop(&File->clients_fd, NULL, &fd_client);
            File->lockFd = fd_client;
            SYSCALL_RET(pthread_unlock, pthread_mutex_unlock(&(File->lock)),
                           "ERROR - Unlock file %s failure", File->path)
            SYSCALL_RET(pthred_unlock, pthread_mutex_unlock(&(ServerStorage->lock)),
                           "ERROR - ServerStorage lock release failure, errno = %d", errno)
            if(ServerStorage->stdOutput) printf("[Thread %d]: Server UnLock\n", workerId);

            fileWritersDecrement(File, workerId);
            lastOpUpdate(File, O_LOCK, workerId);
            message1->feedback = SUCCESS;
            if(ServerStorage->stdOutput) printf("[Thread %d]: File %s locked by client %d\n", workerId, File->path, fd_client);
            appendOnLog(ServerLog,"[Thread %d]: File %s locked by client %d\n", workerId, File->path, fd_client);
            return;
        }
        case (O_PEN):{
            if(File==NULL){
                SYSCALL_RET(pthred_unlock, pthread_mutex_unlock(&(ServerStorage->lock)),
                               "ERROR - ServerStorage lock release failure, errno = %d", errno)
                if(ServerStorage->stdOutput) printf("[Thread %d]: Server UnLock\n", workerId);

                errno = ENOENT;
                message1->additional = errno;
                message1->feedback = ERROR;
                return ;
            }
            SYSCALL_RET(pthred_unlock, pthread_mutex_unlock(&(ServerStorage->lock)),
                           "ERROR - ServerStorage lock release failure, errno = %d", errno)
            if(ServerStorage->stdOutput) printf("[Thread %d]: Server UnLock\n", workerId);

            if(isLocked(File, fd_client)==1 || File->latsOp==O_CREAT){
                errno = EBUSY;
                message1->additional = errno;
                message1->feedback = ERROR;
                SYSCALL_RET(pthred_unlock, pthread_mutex_unlock(&(ServerStorage->lock)),
                               "ERROR - ServerStorage lock release failure, errno = %d", errno)
                if(ServerStorage->stdOutput) printf("[Thread %d]: Server UnLock\n", workerId);

                return;
            }

            fileWritersIncrement(File, workerId);

            if(searchInt(File->clients_fd->head, fd_client)==0){
                pushTop(&File->clients_fd, NULL, &fd_client);
            }
            fileWritersDecrement(File, workerId);
            message1->feedback = SUCCESS;
            if(ServerStorage->stdOutput) printf("[Thread %d]: File %s opened by client %d\n", workerId, File->path, fd_client);
            appendOnLog(ServerLog,"[Thread %d]: File %s opened by client %d\n", workerId, File->path, fd_client);
            return ;
        }
        default: return;
    }
    return;
}
void CloseFile(int fd_client, int workerId, message* message1){
    if(fd_client == -1 || workerId == -1) {
        errno = EINVAL;
        message1->additional = errno;
        message1->feedback = ERROR;
        return;
    }

    SYSCALL_RET(pthred_lock, pthread_mutex_lock(&(ServerStorage->lock)),
                   "ERROR - ServerStorage lock acquisition failure, errno = %d", errno)
    if(ServerStorage->stdOutput) printf("[Thread %d]: Server Lock\n", workerId);

    serverFile* File = icl_hash_find(ServerStorage->filesTable, message1->content);
    if(File==NULL || isLocked(File, fd_client)==1 || File->latsOp==O_CREAT){
        errno = ENOENT;
        message1->additional = errno;
        message1->feedback = ERROR;
        SYSCALL_RET(pthred_unlock, pthread_mutex_unlock(&(ServerStorage->lock)),
                       "ERROR - ServerStorage lock acquisition failure, errno = %d", errno)
        if(ServerStorage->stdOutput) printf("[Thread %d]: Server UnLock\n", workerId);

        return;
    }
    fileWritersIncrement(File, workerId);

    SYSCALL_RET(pthred_unlock, pthread_mutex_unlock(&(ServerStorage->lock)),
                "ERROR - ServerStorage lock acquisition failure, errno = %d", errno)
    if(ServerStorage->stdOutput) printf("[Thread %d]: Server UnLock\n", workerId);


    if(searchInt(File->clients_fd->head, fd_client)==1 ){
        removeByInt(&File->clients_fd, &fd_client);
    } else{
        message1->feedback = ERROR;
        message1->additional = 13;
        fileWritersDecrement(File, workerId);
        return;
    }
    message1->feedback = SUCCESS;
    if(ServerStorage->stdOutput) printf("[Thread %d]: File %s closed by client %d\n", workerId, File->path, fd_client);
    appendOnLog(ServerLog, "[Thread %d]: File %s closed by client %d\n", workerId, File->path, fd_client);
    fileWritersDecrement(File, workerId);
    return;
}
void DeleteFile(int fd_client, int workerId, message* message1){
    if(fd_client == -1 || workerId == -1) {
        errno = EINVAL;
        message1->additional = errno;
        message1->feedback = ERROR;
        return;
    }


    SYSCALL_RET(pthred_lock, pthread_mutex_lock(&(ServerStorage->lock)),
                   "ERROR - ServerStorage lock acquisition failure, errno = %d", errno)
    if(ServerStorage->stdOutput) printf("[Thread %d]: Server Lock\n", workerId);

    serverFile* File = icl_hash_find(ServerStorage->filesTable, message1->content);
    if(File==NULL){
        errno = ENOENT;
        message1->additional = errno;
        message1->feedback = ERROR;
        SYSCALL_RET(pthred_unlock, pthread_mutex_unlock(&(ServerStorage->lock)),
                       "ERROR - ServerStorage lock acquisition failure, errno = %d", errno)
        if(ServerStorage->stdOutput) printf("[Thread %d]: Server UnLock\n", workerId);

        return ;
    }
    fileWritersIncrement(File, workerId);
    if((isLocked(File, fd_client)==1) || File->latsOp==O_CREAT || File->clients_fd->len>1){
        errno = EACCES;
        message1->additional = errno;
        message1->feedback = ERROR;
        SYSCALL_RET(pthred_unlock, pthread_mutex_unlock(&(ServerStorage->lock)),
                    "ERROR - ServerStorage lock acquisition failure, errno = %d", errno)
        if(ServerStorage->stdOutput) printf("[Thread %d]: Server UnLock\n", workerId);

        fileWritersDecrement(File, workerId);
        return ;
    }
    fileBytesDecrement(File->size);
    ServerStorage->deletedBytes+=File->size;
    fileWritersDecrement(File, workerId);
    if(icl_hash_delete(ServerStorage->filesTable, message1->content, free, freeFile) == -1){
        message1->additional = 132;
        message1->feedback = ERROR;
        SYSCALL_RET(pthred_unlock, pthread_mutex_unlock(&(ServerStorage->lock)),
                       "ERROR - ServerStorage lock acquisition failure, errno = %d", errno)
        if(ServerStorage->stdOutput) printf("[Thread %d]: Server UnLock\n", workerId);

        return;
    }
    fileNumDecrement();
    ServerStorage->deletedFiles++;
    SYSCALL_RET(pthred_unlock, pthread_mutex_unlock(&(ServerStorage->lock)),
                   "ERROR - ServerStorage lock acquisition failure, errno = %d", errno);
    if(ServerStorage->stdOutput) printf("[Thread %d]: Server UnLock\n", workerId);

    message1->feedback = SUCCESS;
    if(ServerStorage->stdOutput) printf("[Thread %d]: File %s deleted by client %d\n", workerId, (char*)message1->content, fd_client);
    appendOnLog(ServerLog,"[Thread %d]: File %s deleted by client %d\n", workerId, message1->content, fd_client);
    //freeMessageContent(message1);
    return;
}
void ReceiveFile(int fd_client, int workerId, message* message1){
    if(fd_client == -1 || workerId == -1) {
        errno = EINVAL;
        message1->additional = errno;
        message1->feedback = ERROR;
        return;
    }

    SYSCALL_RET(pthred_lock, pthread_mutex_lock(&(ServerStorage->lock)),
                   "ERROR - ServerStorage lock acquisition failure, errno = %d", errno)
    if(ServerStorage->stdOutput) printf("[Thread %d]: Server Lock\n", workerId);


    serverFile* File = (message1->size>0)? icl_hash_find(ServerStorage->filesTable, message1->content):NULL;
    if(File==NULL){
        errno = ENOENT;
        message1->additional = errno;
        message1->feedback = ERROR;
        SYSCALL_RET(pthred_unlock, pthread_mutex_unlock(&(ServerStorage->lock)),
                       "ERROR - ServerStorage lock acquisition failure, errno = %d", errno)
        if(ServerStorage->stdOutput) printf("[Thread %d]: Server UnLock\n", workerId);

        return ;
    }
    fileWritersIncrement(File, workerId);


    if(File->content!=NULL || searchInt(File->clients_fd->head, fd_client)==0 ||
            isLocked(File, fd_client)==1 || File->latsOp!=O_CREAT){
        // File already exists, use append to add new content
        fileWritersDecrement(File, workerId);
        message1->additional = EACCES;
        message1->feedback = ERROR;
        SYSCALL_RET(pthred_unlock, pthread_mutex_unlock(&(ServerStorage->lock)),
                       "ERROR - ServerStorage lock acquisition failure, errno = %d", errno)
        if(ServerStorage->stdOutput) printf("[Thread %d]: Server UnLock\n", workerId);
        return ;
    }

    freeMessageContent(message1);
    message1->feedback = SUCCESS;
    writeMessage(fd_client, message1);

    readMessage(fd_client, message1);

    if(message1->request!=O_DATA){
        message1->additional = errno;
        message1->feedback = ERROR;
        fileWritersDecrement(File, workerId);
        freeMessageContent(message1);
        SYSCALL_RET(pthred_unlock, pthread_mutex_unlock(&(ServerStorage->lock)),
                       "ERROR - ServerStorage lock acquisition failure, errno = %d", errno)
        if(ServerStorage->stdOutput) printf("[Thread %d]: Server UnLock\n", workerId);

        return ;
    }

    if( message1->size > ServerConfig.maxByte){
        errno = EFBIG;
        message1->additional = errno;
        message1->feedback = ERROR;
        fileWritersDecrement(File, workerId);
        freeMessageContent(message1);
        icl_hash_delete(ServerStorage->filesTable, File->path, free, freeFile);
        SYSCALL_RET(pthred_unlock, pthread_mutex_unlock(&(ServerStorage->lock)),
                       "ERROR - ServerStorage lock acquisition failure, errno = %d", errno)
        if(ServerStorage->stdOutput) printf("[Thread %d]: Server UnLock\n", workerId);

        return ;
    }
    List expelled;
    createList(&expelled);
    while (ServerStorage->actualFilesBytes+message1->size > ServerConfig.maxByte){
        serverFile* file = icl_hash_toReplace(ServerStorage->filesTable, ServerConfig.policy, workerId);
        if(file == NULL) {
            message1->additional = 132;
            message1->feedback = ERROR;
            fileWritersDecrement(File, workerId);
            freeMessageContent(message1);
            icl_hash_delete(ServerStorage->filesTable, File->path, free, freeFile);
            SYSCALL_RET(pthred_unlock, pthread_mutex_unlock(&(ServerStorage->lock)),
                           "ERROR - ServerStorage lock acquisition failure, errno = %d", errno)
            if(ServerStorage->stdOutput) printf("[Thread %d]: Server UnLock\n", workerId);

            return ;
        }
        fileBytesDecrement(file->size);
        fileNumDecrement();
        ServerStorage->expelledBytes+=file->size;
        ServerStorage->expelledFiles++;
        file->toDelete = 1;
        pushTop(&expelled, file->path, NULL);
    }

    fileBytesIncrement(message1->size);
    File->size = message1->size;
    File->content = malloc(message1->size);
    memcpy(File->content, message1->content, message1->size);
    int scRes = 0;


    SYSCALL_ASSIGN(pthred_unlock, scRes, pthread_mutex_unlock(&(ServerStorage->lock)),
                   "ERROR - ServerStorage lock acquisition failure, errno = %d", errno);
    if(ServerStorage->stdOutput) printf("[Thread %d]: Server UnLock\n", workerId);

    if(File->size>0)freeMessageContent(message1);
    if(scRes==-1){
        message1->additional = EDEADLK;
        message1->feedback = ERROR;
        fileBytesDecrement(File->size);
        fileNumDecrement();
        fileWritersDecrement(File, workerId);
        icl_hash_delete(ServerStorage->filesTable, File->path, free, freeFile);
        return;
    }
    File->lockFd=-1;
    icl_hash_toDelete(ServerStorage->filesTable, expelled, fd_client, workerId);
    fileWritersDecrement(File, workerId);
    if(ServerStorage->stdOutput) printf("[Thread %d]: Server UnLock\n", workerId);
    lastOpUpdate(File, O_WRITE, workerId);

    message1->feedback = SUCCESS;
    message1->additional = expelled->len;
    writeMessage(fd_client, message1);

    if(ServerStorage->stdOutput) printf("[Thread %d]: File %s received by client %d\n", workerId, File->path, fd_client);
    appendOnLog(ServerLog, "[Thread %d]: File %s received from client %d\n", workerId, File->path, fd_client);
    if(ExpelledHandler(fd_client, workerId, expelled)==-1){
        deleteList(&expelled);
        errno = EBADMSG;
        message1->additional = errno;
        message1->request = O_WRITE;
        message1->feedback = ERROR;
        icl_hash_delete(ServerStorage->filesTable, File->path, free, freeFile);
        return ;
    }
    message1->feedback = SUCCESS;
    message1->request = O_WRITE;
    return ;
}
void SendFile(int fd_client, int workerId, message* message1){
    if(fd_client == -1 || workerId == -1) {
        errno = EINVAL;
        message1->additional = errno;
        message1->feedback = ERROR;
        return;
    }

    SYSCALL_RET(pthred_lock, pthread_mutex_lock(&(ServerStorage->lock)),
                   "ERROR - ServerStorage lock acquisition failure, errno = %d", errno)
    if(ServerStorage->stdOutput) printf("[Thread %d]: Server Lock\n", workerId);

    serverFile* File = icl_hash_find(ServerStorage->filesTable, message1->content);
    if(message1->content) freeMessageContent(message1);
    if(File==NULL){
        errno = ENOENT;
        message1->additional = errno;
        message1->feedback = ERROR;
        SYSCALL_RET(pthred_unlock, pthread_mutex_unlock(&(ServerStorage->lock)),
                       "ERROR - ServerStorage lock acquisition failure, errno = %d", errno)
        if(ServerStorage->stdOutput) printf("[Thread %d]: Server UnLock\n", workerId);

        return;
    }
    fileReadersIncrement(File, workerId);

    if((File->lockFd != -1 && File->lockFd != fd_client) || File->latsOp==O_CREAT){
        errno = EACCES;
        message1->additional = errno;
        message1->feedback = ERROR;
        fileReadersDecrement(File,workerId);
        SYSCALL_RET(pthred_unlock, pthread_mutex_unlock(&(ServerStorage->lock)),
                       "ERROR - ServerStorage lock acquisition failure, errno = %d", errno)
        if(ServerStorage->stdOutput) printf("[Thread %d]: Server UnLock\n", workerId);

        return;
    }
    if(searchInt(File->clients_fd->head, fd_client)==0){
        errno = EACCES;
        message1->additional = errno;
        message1->feedback = ERROR;
        fileReadersDecrement(File,workerId);
        SYSCALL_RET(pthred_unlock, pthread_mutex_unlock(&(ServerStorage->lock)),
                    "ERROR - ServerStorage lock acquisition failure, errno = %d", errno)
        if(ServerStorage->stdOutput) printf("[Thread %d]: Server UnLock\n", workerId);

        return;
    }
    SYSCALL_RET(pthred_unlock, pthread_mutex_unlock(&(ServerStorage->lock)),
                   "ERROR - ServerStorage lock acquisition failure, errno = %d", errno)
    if(ServerStorage->stdOutput) printf("[Thread %d]: Server UnLock\n", workerId);


    message1->feedback = SUCCESS;
    writeMessage(fd_client, message1);

    message1->content = malloc(File->size);
    memcpy(message1->content, File->content, File->size);
    message1->size = File->size;
    message1->feedback = SUCCESS;
    message1->request = O_DATA;

    fileReadersDecrement(File,workerId);
    lastOpUpdate(File, O_READ, workerId);
    if(ServerStorage->stdOutput) printf("[Thread %d]: File %s sent to client %d\n", workerId, File->path, fd_client);
    appendOnLog(ServerLog, "[Thread %d]: File %s sent to client %d\n", workerId, File->path, fd_client);
    return;
}
void SendNFiles(int fd_client, int workerId, message* message1){
    if(fd_client == -1 || workerId == -1) {
        errno = EINVAL;
        message1->additional = errno;
        message1->feedback = ERROR;
        return;
    }

    int numberOfFiles = 0;
    SYSCALL_RET(pthred_lock, pthread_mutex_lock(&(ServerStorage->lock)),
                   "ERROR - ServerStorage lock acquisition failure, errno = %d", errno)
    if(ServerStorage->stdOutput) printf("[Thread %d]: Server Lock\n", workerId);

    numberOfFiles = (ServerStorage->actualFilesNumber<message1->additional)? ServerStorage->actualFilesNumber:message1->additional;
    List expelled;
    createList(&expelled);

    icl_entry_t *bucket, *curr;
    serverFile* file;
    for(int i = 0; i < ServerStorage->filesTable->nbuckets && (int)expelled->len<numberOfFiles; i++){
        bucket = ServerStorage->filesTable->buckets[i];
        for(curr = bucket; curr!=NULL && (int)expelled->len<numberOfFiles; curr=curr->next){
            file = (serverFile*) curr->data;
            if(file->latsOp==O_CREAT) continue;
            if(file->writers!=0) continue;
            fileReadersIncrement(file, workerId);
            if(isLocked(file, fd_client)==1 ) {
                fileReadersDecrement(file,workerId);
                continue;
            }
            if(pushTop(&expelled, file->path, NULL) == -1){
                message1->additional = errno;
                message1->feedback = ERROR;
                fprintf(stderr, "ERROR - List push failure for sendNFiles, %d", errno);
                SYSCALL_RET(pthred_unlock, pthread_mutex_unlock(&(ServerStorage->lock)),
                            "ERROR - ServerStorage lock release failure, errno = %d", errno)
                if(ServerStorage->stdOutput) printf("[Thread %d]: Server UnLock\n", workerId);
                fileReadersDecrement(file,workerId);
                return;
            }
            fileReadersDecrement(file,workerId);
        }
    }

    SYSCALL_RET(pthred_unlock, pthread_mutex_unlock(&(ServerStorage->lock)),
                   "ERROR - ServerStorage lock release failure, errno = %d", errno)
    if(ServerStorage->stdOutput) printf("[Thread %d]: Server UnLock\n", workerId);


    numberOfFiles = expelled->len;
    emptyMessage(message1);
    message1->feedback = SUCCESS;
    message1->additional = expelled->len;
    writeMessage(fd_client, message1);
    if(ServerStorage->stdOutput) printf("[Thread %d]: Send %d files to client %d\n", workerId, numberOfFiles, fd_client);
    appendOnLog(ServerLog, "[Thread %d]: Send %d files to client %d\n", workerId, numberOfFiles, fd_client);
    message1->additional = numberOfFiles;
    if(ExpelledHandler(fd_client, workerId, expelled)!=0){
        deleteList(&expelled);
        errno = EBADMSG;
        message1->additional = errno;
        message1->feedback = ERROR;
        return ;
    }
    message1->feedback = SUCCESS;
    return;
}
void LockFile(int fd_client, int workerId, message* message1){
    if(fd_client == -1 || workerId == -1) {
        errno = EINVAL;
        message1->additional = errno;
        message1->feedback = ERROR;
        return;
    }

    SYSCALL_RET(pthred_lock, pthread_mutex_lock(&(ServerStorage->lock)),
                   "ERROR - ServerStorage lock acquisition failure, errno = %d", errno)
    if(ServerStorage->stdOutput) printf("[Thread %d]: Server Lock\n", workerId);

    serverFile* File = icl_hash_find(ServerStorage->filesTable, message1->content);
    if(File==NULL){
        errno = ENOENT;
        message1->additional = errno;
        message1->feedback = ERROR;
        SYSCALL_RET(pthred_unlock, pthread_mutex_unlock(&(ServerStorage->lock)),
                       "ERROR - ServerStorage lock acquisition failure, errno = %d", errno)
        if(ServerStorage->stdOutput) printf("[Thread %d]: Server UnLock\n", workerId);

        return;
    }
    fileWritersIncrement(File, workerId);
    SYSCALL_RET(pthred_unlock, pthread_mutex_unlock(&(ServerStorage->lock)),
                   "ERROR - ServerStorage lock acquisition failure, errno = %d", errno)
    if(ServerStorage->stdOutput) printf("[Thread %d]: Server UnLock\n", workerId);

    if(searchInt(File->clients_fd->head, fd_client)==0 || File->latsOp==O_CREAT){
        errno = EACCES;
        message1->additional = errno;
        message1->feedback = ERROR;
        fileWritersDecrement(File, workerId);
        return;
    }
    if(File->lockFd!=-1){
        if(File->lockFd != fd_client){
            errno = EBUSY;
            message1->additional = errno;
            message1->feedback = ERROR;
            fileWritersDecrement(File, workerId);
            return;
        }
    }
    File->lockFd = fd_client;
    fileWritersDecrement(File, workerId);
    lastOpUpdate(File,O_LOCK, workerId);
    message1->feedback = SUCCESS;
    if(ServerStorage->stdOutput) printf("[Thread %d]: File %s locked by client %d\n", workerId, File->path, fd_client);
    appendOnLog(ServerLog, "[Thread %d]: File %s locked by client %d\n", workerId, File->path, fd_client);
    return;
}
void UnLockFile(int fd_client, int workerId, message* message1){
    if(fd_client == -1 || workerId == -1) {
        errno = EINVAL;
        message1->additional = errno;
        message1->feedback = ERROR;
        return;
    }

    SYSCALL_RET(pthred_lock, pthread_mutex_lock(&(ServerStorage->lock)),
                   "ERROR - ServerStorage lock acquisition failure, errno = %d", errno)
    if(ServerStorage->stdOutput) printf("[Thread %d]: Server Lock\n", workerId);

    serverFile* File = icl_hash_find(ServerStorage->filesTable, message1->content);
    if(File==NULL){
        errno = ENOENT;
        message1->additional = errno;
        message1->feedback = ERROR;
        SYSCALL_RET(pthred_unlock, pthread_mutex_unlock(&(ServerStorage->lock)),
                       "ERROR - ServerStorage lock acquisition failure, errno = %d", errno)
        if(ServerStorage->stdOutput) printf("[Thread %d]: Server UnLock\n", workerId);

        return;
    }
    fileWritersIncrement(File, workerId);
    SYSCALL_RET(pthred_unlock, pthread_mutex_unlock(&(ServerStorage->lock)),
                   "ERROR - ServerStorage lock acquisition failure, errno = %d", errno)
    if(ServerStorage->stdOutput) printf("[Thread %d]: Server UnLock\n", workerId);

    if(searchInt(File->clients_fd->head, fd_client)==0 || File->latsOp==O_CREAT){
        errno = EACCES;
        message1->additional = errno;
        message1->feedback = ERROR;
        fileWritersDecrement(File, workerId);
        return;
    }
    if(File->lockFd!=-1){
        if(File->lockFd != fd_client){
            errno = EBUSY;
            message1->additional = errno;
            message1->feedback = ERROR;
            fileWritersDecrement(File, workerId);
            return;
        }
    }
    if(File->lockFd==-1){
        errno = EPERM;
        message1->additional = errno;
        message1->feedback = ERROR;
        fileWritersDecrement(File, workerId);
        return;
    }
    File->lockFd = -1;
    fileWritersDecrement(File, workerId);
    lastOpUpdate(File,O_UNLOCK, workerId);
    message1->feedback = SUCCESS;
    if(ServerStorage->stdOutput) printf("[Thread %d]: File %s unlocked by client %d\n", workerId, File->path, fd_client);
    appendOnLog(ServerLog, "[Thread %d]: File %s unlocked by client %d\n", workerId, File->path, fd_client);
    return;
}
void AppendOnFile(int fd_client, int workerId, message* message1){
    if(fd_client == -1 || workerId == -1) {
        errno = EINVAL;
        message1->additional = errno;
        message1->feedback = ERROR;
        return;
    }

    SYSCALL_RET(pthred_lock, pthread_mutex_lock(&(ServerStorage->lock)),
                   "ERROR - ServerStorage lock acquisition failure, errno = %d", errno)
    if(ServerStorage->stdOutput) printf("[Thread %d]: Server Lock\n", workerId);

    serverFile* File = icl_hash_find(ServerStorage->filesTable, message1->content);
    if(File==NULL){
        errno = ENOENT;
        message1->additional = errno;
        message1->feedback = ERROR;
        SYSCALL_RET(pthred_unlock, pthread_mutex_unlock(&(ServerStorage->lock)),
                       "ERROR - ServerStorage lock acquisition failure, errno = %d", errno)
        if(ServerStorage->stdOutput) printf("[Thread %d]: Server UnLock\n", workerId);

        return;
    }
    fileWritersIncrement(File, workerId);

    if(searchInt(File->clients_fd->head, fd_client)==0 || (File->lockFd!=-1 && File->lockFd!=fd_client)
        || File->latsOp==O_CREAT){
        // File already exists, use append to add new content
        fileWritersDecrement(File, workerId);
        message1->additional = EPERM;
        message1->feedback = ERROR;
        SYSCALL_RET(pthred_unlock, pthread_mutex_unlock(&(ServerStorage->lock)),
                       "ERROR - ServerStorage lock acquisition failure, errno = %d", errno)
        if(ServerStorage->stdOutput) printf("[Thread %d]: Server UnLock\n", workerId);

        return;
    }

    emptyMessage(message1);
    message1->feedback = SUCCESS;
    writeMessage(fd_client, message1);

    readMessage(fd_client, message1);
    if(message1->request!=O_DATA){
        message1->additional = 132;
        message1->feedback = ERROR;
        fileWritersDecrement(File, workerId);
        SYSCALL_RET(pthred_unlock, pthread_mutex_unlock(&(ServerStorage->lock)),
                       "ERROR - ServerStorage lock acquisition failure, errno = %d", errno)
        if(ServerStorage->stdOutput) printf("[Thread %d]: Server UnLock\n", workerId);

        return;
    }

    if( message1->size > ServerConfig.maxByte){
        errno = EFBIG;
        message1->additional = errno;
        message1->feedback = ERROR;
        fileWritersDecrement(File, workerId);
        SYSCALL_RET(pthred_unlock, pthread_mutex_unlock(&(ServerStorage->lock)),
                       "ERROR - ServerStorage lock acquisition failure, errno = %d", errno)
        if(ServerStorage->stdOutput) printf("[Thread %d]: Server UnLock\n", workerId);

        return;
    }

    List expelled;
    createList(&expelled);
    while (ServerStorage->actualFilesBytes-File->size+message1->size > ServerConfig.maxByte){
        serverFile* file = icl_hash_toReplace(ServerStorage->filesTable, ServerConfig.policy, workerId);
        if(file == NULL) {
            message1->additional = 132;
            message1->feedback = ERROR;
            fileWritersDecrement(File, workerId);
            SYSCALL_RET(pthred_unlock, pthread_mutex_unlock(&(ServerStorage->lock)),
                           "ERROR - ServerStorage lock acquisition failure, errno = %d", errno)
            if(ServerStorage->stdOutput) printf("[Thread %d]: Server UnLock\n", workerId);

            return;
        }
        fileBytesDecrement(file->size);
        fileNumDecrement();
        ServerStorage->expelledBytes+= file->size;
        ServerStorage->expelledFiles++;
        file->toDelete = 1;
        pushTop(&expelled, file->path, NULL);
    }

    fileBytesIncrement(message1->size);

    File->size = message1->size;
    File->content = malloc(message1->size+1);
    if(File->content!=NULL) free(File->content);
    memcpy(File->content, message1->content, message1->size);
    fileWritersDecrement(File, workerId);
    lastOpUpdate(File, O_APPND, workerId);
    // Success till here
    emptyMessage(message1);
    message1->feedback = SUCCESS;
    message1->additional = expelled->len;
    writeMessage(fd_client, message1);

    SYSCALL_RET(pthred_unlock, pthread_mutex_unlock(&(ServerStorage->lock)),
                "ERROR - ServerStorage lock acquisition failure, errno = %d", errno)
    if(ServerStorage->stdOutput) printf("[Thread %d]: Server UnLock\n", workerId);
    if(ServerStorage->stdOutput) printf("[Thread %d]: File %s updated by client %d\n", workerId, File->path, fd_client);
    appendOnLog(ServerLog, "[Thread %d]: File %s updated by client %d\n", workerId, File->path, fd_client);
    if(ExpelledHandler(fd_client, workerId, expelled)==-1){
        deleteList(&expelled);
        errno = EBADMSG;
        message1->additional = errno;
        message1->feedback = ERROR;
        return;
    }
    message1->feedback = SUCCESS;
    return;
}

int ExpelledHandler(int fd, int workerId, List expelled){
    if(expelled==NULL) return -1;
    if(expelled->len==0){
        deleteList(&expelled);
        return 0;
    }
    message curr;
    memset(&curr, 0, sizeof(message));
    int num = expelled->len;
    char* index;
    int len;
    int scRes=0;
    serverFile* File;
    SYSCALL_RETURN(pthred_unlock, pthread_mutex_lock(&(ServerStorage->lock)),
                   "ERROR - ServerStorage lock acquisition failure, errno = %d", errno)
    if(ServerStorage->stdOutput) printf("[Thread %d]: Server Lock\n", workerId);
    scRes = readMessage(fd, &curr);
    if(scRes==-1){
        curr.feedback=ERROR;
        curr.additional=EBADMSG;
        SYSCALL_RETURN(pthred_unlock, pthread_mutex_unlock(&(ServerStorage->lock)),
                       "ERROR - ServerStorage lock acquisition failure, errno = %d", errno)
        if(ServerStorage->stdOutput) printf("[Thread %d]: Server UnLock\n", workerId);
        return -1;
    } else if(curr.feedback==SUCCESS && curr.additional==0){
        while (expelled->len>0){
            pullTop(&expelled, &index, NULL);
            File = icl_hash_find(ServerStorage->filesTable, index);
            if(File && File->toDelete==1 && isLocked(File, fd)!=1 && File->latsOp!=O_CREAT) {
                icl_hash_delete(ServerStorage->filesTable, File->path, free, freeFile);
                if(ServerStorage->stdOutput) printf("[Thread %d]: File %s expelled, %d of %d files\n", workerId, index, num-expelled->len, num);
                appendOnLog(ServerLog, "[Thread %d]: File %s expelled, %d of %d files\n", workerId, index, num-expelled->len, num);
            }
            free(index);
        }
    }
    freeMessageContent(&curr);
    while (expelled->len>0){
        pullTop(&expelled, &index, NULL);
        if((File = icl_hash_find(ServerStorage->filesTable, index)) != NULL){
            if(File->latsOp == O_CREAT || isLocked(File, fd)==1){
                free(index);
                continue;
            }
        } else  {
            free(index);
            continue;
        }
        len = strlen(index)+1;

        curr.size = len;
        curr.request = O_SEND;
        curr.content = malloc(len);
        strncpy(curr.content, index, len);
        scRes = writeMessage(fd, &curr);
        if(scRes==-1){
            curr.feedback=ERROR;
            curr.additional=EBADMSG;
            freeMessageContent(&curr);
            SYSCALL_RETURN(pthred_unlock, pthread_mutex_unlock(&(ServerStorage->lock)),
                           "ERROR - ServerStorage lock acquisition failure, errno = %d", errno)
            if(ServerStorage->stdOutput) printf("[Thread %d]: Server UnLock\n", workerId);
            return -1;
        }
        freeMessageContent(&curr);

        scRes = readMessage(fd, &curr);
        if(curr.feedback != SUCCESS ||  scRes==-1) {
            curr.feedback = ERROR;
            curr.additional = 132;
            free(index);
            freeMessageContent(&curr);
            SYSCALL_RETURN(pthred_unlock, pthread_mutex_unlock(&(ServerStorage->lock)),
                           "ERROR - ServerStorage lock acquisition failure, errno = %d", errno)
            if(ServerStorage->stdOutput) printf("[Thread %d]: Server UnLock\n", workerId);
            return -1;
        }

        freeMessageContent(&curr);


        fileWritersIncrement(File, workerId);
        len = (int)File->size;

        curr.size = File->size;
        curr.content = malloc(File->size);
        memcpy(curr.content, File->content, File->size);
        curr.request = O_DATA;
        curr.feedback = SUCCESS;
        scRes = writeMessage(fd, &curr);
        freeMessageContent(&curr);
        if(scRes==-1){
            curr.feedback=ERROR;
            curr.additional=EBADMSG;
            fileWritersDecrement(File, workerId);
            SYSCALL_RETURN(pthred_unlock, pthread_mutex_unlock(&(ServerStorage->lock)),
                           "ERROR - ServerStorage lock acquisition failure, errno = %d", errno)
            if(ServerStorage->stdOutput) printf("[Thread %d]: Server UnLock\n", workerId);
            return -1;
        }

        fileWritersDecrement(File, workerId);
        if(File->toDelete==1) {
            icl_hash_delete(ServerStorage->filesTable, index, free, freeFile);
            if(ServerStorage->stdOutput) printf("[Thread %d]: File %s expelled, %d of %d files sent to client %d\n", workerId, index, num-expelled->len, num, fd);
            appendOnLog(ServerLog, "[Thread %d]: File %s expelled, %d of %d files sent to client %d\n", workerId, index, num-expelled->len, num, fd);
        }
        else {
            lastOpUpdate(File, O_READ, workerId);
            if(ServerStorage->stdOutput) printf("[Thread %d]: File %s, %d of %d files sent to client %d\n", workerId, index, num-expelled->len, num, fd);
            appendOnLog(ServerLog, "[Thread %d]: File %s, %d of %d files sent to client %d\n", workerId, index, num-expelled->len, num, fd);
        }

        free(index);
    }
    SYSCALL_RETURN(pthred_unlock, pthread_mutex_unlock(&(ServerStorage->lock)),
                   "ERROR - ServerStorage lock acquisition failure, errno = %d", errno)

    if(ServerStorage->stdOutput) printf("[Thread %d]: Server UnLock\n", workerId);
    deleteList(&expelled);
    return 0;
}

int fileReadersIncrement(serverFile* file, int thread){
    SYSCALL_RETURN(lock, pthread_mutex_lock(&file->order), "ERROR - Mutex lock of file failed, %s\n", file->path);
    SYSCALL_RETURN(lock, pthread_mutex_lock(&file->lock), "ERROR - Mutex lock of file failed, %s\n", file->path);
    while (file->writers!=0) {
        SYSCALL_RETURN(cond_wait, pthread_cond_wait(&(file->condition), &(file->lock)),
                       "ERROR - Condition wait for file %s failed\n", file->path);
    }
    file->readers++;
    SYSCALL_RETURN(unlock, pthread_mutex_unlock(&(file->order)),
                   "ERROR - Mutex unlock of file failed, %s\n", file->path);
    SYSCALL_RETURN(unlock, pthread_mutex_unlock(&(file->lock)),
                   "ERROR - Mutex unlock of file failed, %s\n", file->path);
    if(ServerStorage->stdOutput) fprintf(stdout, "[THREAD %d]: READ Increment: File %s\n", thread, file->path);
    return 0;
}
int fileReadersDecrement(serverFile* file, int thread){
    SYSCALL_RETURN(lock, pthread_mutex_lock(&file->lock), "ERROR - Mutex lock of file failed, %s\n", file->path);
    file->readers--;
    if(file->readers == 0) {
        SYSCALL_RETURN(cond_broad, pthread_cond_broadcast(&(file->condition)),
                       "ERROR - Condition broadcast reader release file %s", file->path);
    }
    SYSCALL_RETURN(unlock, pthread_mutex_unlock(&(file->lock)),
                   "ERROR - Mutex unlock of file failed, %s\n", file->path);
    if(ServerStorage->stdOutput) fprintf(stdout, "[THREAD %d]: READ Decrement: File %s\n", thread, file->path);
    return 0;
}
int fileWritersIncrement(serverFile* file, int thread){
    SYSCALL_RETURN(lock, pthread_mutex_lock(&file->order), "ERROR - Mutex lock of file failed, %s\n", file->path);
    SYSCALL_RETURN(lock, pthread_mutex_lock(&file->lock), "ERROR - Mutex lock of file failed, %s\n", file->path);
    while (file->writers!=0 || file->readers!=0) {
        SYSCALL_RETURN(cond_wait, pthread_cond_wait(&(file->condition), &(file->lock)),
                       "ERROR - Condition wait for file %s failed\n", file->path);
    }
    file->writers++;
    SYSCALL_RETURN(unlock, pthread_mutex_unlock(&(file->order)),
                   "ERROR - Mutex unlock of file failed, %s\n", file->path);
    SYSCALL_RETURN(unlock, pthread_mutex_unlock(&(file->lock)),
                   "ERROR - Mutex unlock of file failed, %s\n", file->path);
    if(ServerStorage->stdOutput) fprintf(stdout, "[THREAD %d]: WRITE Increment: File %s lock\n", thread, file->path);
    return 0;
}
int fileWritersDecrement(serverFile* file, int thread){
    SYSCALL_RETURN(lock, pthread_mutex_lock(&file->lock), "ERROR - Mutex lock of file failed, %s\n", file->path);
    SYSCALL_RETURN(cond_broad, pthread_cond_broadcast(&(file->condition)),
                   "ERROR - Condition broadcast reader release file %s", file->path);
    file->writers--;
    SYSCALL_RETURN(unlock, pthread_mutex_unlock(&(file->lock)),
                   "ERROR - Mutex unlock of file failed, %s\n", file->path);
    if(ServerStorage->stdOutput) fprintf(stdout, "[THREAD %d]: WRITE Decrement: File %s lock release\n", thread, file->path);
    return 0;
}
int isLocked(serverFile* file, long fd) {
    SYSCALL_RETURN(lock, pthread_mutex_lock(&file->lock), "ERROR - Mutex lock of file failed, %s\n", file->path);
    if(file->lockFd == fd){
        SYSCALL_RETURN(unlock, pthread_mutex_unlock(&(file->lock)),
                       "ERROR - Mutex unlock of file failed, %s\n", file->path);
        return 0;
    }if(file->lockFd!=-1){
        SYSCALL_RETURN(unlock, pthread_mutex_unlock(&(file->lock)),
                       "ERROR - Mutex unlock of file failed, %s\n", file->path);
        return 1;
    }
    SYSCALL_RETURN(unlock, pthread_mutex_unlock(&(file->lock)),
                   "ERROR - Mutex unlock of file failed, %s\n", file->path);
    return -1;
}
int lastOpUpdate(serverFile* file, fileFlags op, int thread){
    if(fileWritersIncrement(file, thread)==-1) return -1;
    file->latsOp = op;
    if(ServerStorage->stdOutput) fprintf(stdout, "[THREAD %d]: OP UPDATE: File %s %s\n", thread, file->path,
                                         requestToString(file->latsOp));
    if(clock_gettime(CLOCK_REALTIME, &(file->lastOpTime))==-1){
        return -1;
    }
    if(fileWritersDecrement(file, thread)==-1) return -1;
    return 0;
}

wTask* taskCreate (int client_fd){
    if(client_fd == -1) return NULL;
    wTask *task;
    task = (wTask*)malloc(sizeof(wTask));
    task->pipeT = -1;
    task->worker_id = -1;
    task->client_fd = client_fd;
    return task;
}
int taskDestroy(wTask* task){
    if(task != NULL) {
        free(task);
        return 0;
    }
    return -1;
}
