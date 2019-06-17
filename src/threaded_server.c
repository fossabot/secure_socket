/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (C) 2017-2018 Bytemare <d@bytema.re>. All Rights Reserved.
 */

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>


/* secure_socket */
#include <context.h>
#include <threaded_server.h>
#include <ipc_socket.h>

/* BSD */
#include <sys/fcntl.h>
#include <bsd/libutil.h>
#include <tools.h>
#include <handler.h>


/**
 * Thread handler : executes associated action for client communication
 * @param args
 * @return
 */
static void* handle_client(void *args){

    thread_context *ctx;

    LOG_INIT

    ctx = (thread_context*)args;

    LOG(LOG_TRACE, "Thread launched. Calling handler.", 0, 0, ctx->log)

    /**
     * Launch your client handler here, e.g. :
     * - handle_client_connection(client)
     * - handler(ctx)
     */
    handler(ctx);

    LOG(LOG_TRACE, "Connexion closed. Thread now exiting.", 0, 0, ctx->log)

    // TODO : what happens with the context, does this thread need to clean up everything or does the server do something ?

    pthread_exit((void*)0);
}


/**
 * Given a path to filename, reads the file and returns an appropriate buffer containing its content and appropriately
 * sets the size pointer to the number of bytes read.
 * @param filename
 * @param length
 * @return
 */
char* read_data_from_source(const char *filename, int *size, logging *log){

    int file;
    ssize_t length;
    char *destination;
    struct stat file_info;

    LOG_INIT

    LOG_BUILD("Attempting to read data from file '%s' ", filename)
    LOG(LOG_TRACE, NULL, 0, -2, log)

    /*
     * Use of a BSD function here with a lock to prevent a race condition, since the function is used to open a PID file, among others
     */
    file = secure_file_exclusive_open(filename, O_RDONLY, 0);

    if ( log->fd == -1 ){
        if( errno == EWOULDBLOCK){
            LOG_BUILD("Unable to open file '%s', the log file is locked by another process. Free the file and try again.", filename)
            LOG(LOG_ALERT, NULL, errno, 5, log)
        } else {
            LOG_BUILD("Error in opening '%s'for reading.", filename)
            LOG(LOG_TRACE, NULL, 0, 0, log)
        }
        return NULL;
    }

    if ( log->fd == 0 ){
        LOG_STDOUT(LOG_FATAL, "Symlinks for file opening are forbidden (this is either an error or a TOCTOU race condition).", errno, 6, log)
        return NULL;
    }

    LOG_BUILD("Successfully opened file '%s' for reading.", filename)
    LOG(LOG_TRACE, NULL, 0, 0, log)

    fstat(file, &file_info);

    if (!S_ISREG(file_info.st_mode)) {
        LOG_BUILD("Error : '%s' is not a regular file !", filename)
        LOG(LOG_ALERT, NULL, errno, 4, log)
        close(file);
        return NULL;
    }

    length = file_info.st_size;

    LOG_BUILD("File '%s' is '%d' bytes long.", filename, (int)length)
    LOG(LOG_TRACE, NULL, errno, 0, log)

    destination = calloc((unsigned long) (length + 1), sizeof(char));
    if( !destination ){
        // TODO : give filename and buffer size in error
        LOG(LOG_ALERT, "calloc for file reading failed : ", errno, 2, log)
        return NULL;
    }

    length = read(file, destination, sizeof(destination) - 1);
    if (length == -1) {
        // TODO handle error
        close(file);
        return NULL;
    } else {
        if (length != (sizeof(destination) - 1)) {
            // TODO handle this case
            close(file);
            return NULL;
        }
    }
    destination[length] = '\0';

    LOG_BUILD("Read '%d' bytes from '%s'", (int)length, filename)
    LOG(LOG_TRACE, NULL, 0, 3, log)
    close(file);

    *size = (int)length;
    return destination;
}


/**
 * Server function : Daemon mode awaiting connections and executing threads to handle them
 * Accepts up to 10 errors before leaving daemon mode
 * @param server
 * @param nb_cnx
 */
void threaded_server(server_context *ctx, const unsigned int nb_cnx){

    /* List of threads system id's */
    unsigned int count; /* Total accumulated amount of accepted connections */
    unsigned int offset; /* Index between 0 and nb_cnx */
    /*
     * TODO
     * Track number of effectively created threads with unsigned int nb_threads = 0
     */
    unsigned int nb_authorised_errors; /* Number of errors before exiting daemon mode */

    thread_context **client_ctx; /* Contexts with logging queue and ipc_sockets to deal with clients */
    secure_socket *new_client;

    /* Initialise variables */
    pthread_t tid;

    LOG_INIT

    count = 0;
    nb_authorised_errors = 50; /* TODO : study how this situation can be handled in a more appropriate way */

    client_ctx = malloc(nb_cnx * sizeof(thread_context*));
    if( client_ctx == NULL){
        //TODO give size of failed malloc
        LOG(LOG_FATAL, "malloc failed for client/thread_contexts ", errno, 2, ctx->log)
        return;
    }

    LOG(LOG_INFO, "Server now ready and awaiting incoming connections.", 0, 0, ctx->log)

    /* Enter Daemon mode */
    while(nb_authorised_errors) {

        /* get_next_available_offset() */

        offset = count++%(nb_cnx);

        client_ctx[offset] = make_thread_context(NULL, ctx);

        new_client = ipc_accept_connection(ctx);

        if( new_client == NULL ){
            //TODO give more context on why connection was denied
            LOG(LOG_ALERT, "Connection denied.", errno, 3, ctx->log)
            count--;
            nb_authorised_errors--;
            free_thread_context(client_ctx[offset]);
            continue;
        }

        client_ctx[offset]->socket = new_client;

        /* (void*)handle_client */
        if( pthread_create(&tid, &ctx->attr, &handle_client, client_ctx[offset]) != 0 ){
            //TODO give more context on why thread could not be created
            LOG(LOG_ALERT, "error creating thread. Connection closed.", errno, 1, ctx->log)
            client_ctx[offset] = free_thread_context(client_ctx[offset]);
            nb_authorised_errors--;
            continue;
        }

    }

    LOG(LOG_INFO, "Thread Server is quitting daemon mode. Now cleaning up.", 0, 0, ctx->log)

    if( pthread_attr_destroy(&ctx->attr) != 0 ){
        LOG(LOG_ERROR, "Thread Server could not destroy thread attributes.", errno, 1, ctx->log)
    }

    free(client_ctx);
}
