#if !defined(CONN_H)
#define CONN_H

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#define SOCKNAME     "../../tmp/cs_socket"

typedef enum{
    O_PEN = 0,
    O_CREAT = 1,
    O_LOCK = 2,
    O_UNLOCK = 4,
    O_WRITE = 8,
    O_READ = 16,
    O_APPND = 32,
    O_DEL = 64,
    O_CLOS = 128,
    O_DATA = 256,
    O_NULL = 512,
} fileFlags;

typedef enum {
    SUCCESS = 1,
    ERROR = 2,
} fileResponse;

typedef struct{
    int error;
    size_t size;
    void* content;
    fileFlags request;
    fileResponse feedback;
} message;

// Lezione 9 laboratorio, funzione di scrittura e lettura per evitare che rimangano dati sul buffer non gestiti
/** Evita letture parziali
 *
 *   \retval -1   errore (errno settato)
 *   \retval  0   se durante la lettura da fd leggo EOF
 *   \retval size se termina con successo
 */
static inline int readn(long fd, void *buf, size_t size) {
    size_t left = size;
    int r;
    char *bufptr = (char*)buf;
    while(left>0) {
        if ((r=read((int)fd ,bufptr,left)) == -1) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (r == 0) return 0;   // EOF
        left -= r;
        bufptr  += r;
    }
    return size;
}

/** Evita scritture parziali
 *
 *   \retval -1   errore (errno settato)
 *   \retval  0   se durante la scrittura la write ritorna 0
 *   \retval  1   se la scrittura termina con successo
 */
static inline int writen(long fd, void *buf, size_t size) {
    size_t left = size;
    int r;
    char *bufptr = (char*)buf;
    while(left>0) {
	if ((r=write((int)fd ,bufptr,left)) == -1) {
	    if (errno == EINTR) continue;
	    return -1;
	}
	if (r == 0) return 0;  
        left    -= r;
	bufptr  += r;
    }
    return 1;
}


/**  Legge da fd in message1
 *
 *   \retval -1   errore (errno settato)
 *   \retval  0   se durante la lettura ritorna 0
 *   \retval  size se la lettura termina con successo
 */
static int readMessage(int fd, message* message1){
    if(fd == -1 || message1 == NULL){
        errno = EINVAL
        return -1;
    }

    if(read(fd, message1, sizeof(message)) != sizeof(message)){
        return -1;
    }

    message1->content = calloc(1, message1->size+1);
    int readed;
    if((readed = readn(fd, message1->content, message1->size)) == -1){
        fprintf(stderr, "Reading from %d fd", fd);
        return -1;
    }
    return readed;
}
/**  scrive a fd message1
 *
 *   \retval -1   errore (errno settato)
 *   \retval  0   se durante la scrittura ritorna 0
 *   \retval  1   se la scrittura termina con successo
 */
static int writeMessage(int fd, message* message1){
    if(fd == -1 || message1 == NULL){
        errno = EINVAL
        return -1;
    }

    if(write(fd, message1, sizeof(message)) != sizeof(message)){
        return -1;
    }

    message1->content = calloc(1, message1->size+1);
    int written;
    if(message1->size>0) {
        if ((written = writen(fd, message1->content, message1->size)) == -1) {
            fprintf(stderr, "Writing on %d fd", fd);
            return -1;
        }
    }
    return written;
}

/**  Valori default message1
 *
 */
static void emptyMessage(message* message1){
    message1->size = 0;
    message1->error = 0;
    message1->feedback = 0;
    message1->request = O_NULL;

    if(message1->content!=NULL){
        free(message1->content);
        message1->content = NULL;
    }
}
/**  Libero memoria
 *
 */
static void freeMessage(message* message1){
    if(message1->content!=NULL) free(message1->content);
    free(message1);
}


#endif /* CONN_H */
