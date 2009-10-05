/* mongo.c */

#include "mongo.h"

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>

/* ----------------------------
   message stuff
   ------------------------------ */

struct mongo_message * mongo_message_create( int len , int id , int responseTo , int op ){
    struct mongo_message * mm = (struct mongo_message*)malloc( len );
    if ( ! mm )
        return 0;
    mm->len = len;
    if ( id )
        mm->id = id;
    else
        mm->id = rand();
    mm->responseTo = responseTo;
    mm->op = op;
    return mm;
}

/* ----------------------------
   connection stuff
   ------------------------------ */

int mongo_connect( struct mongo_connection * conn , struct mongo_connection_options * options ){
    const char * ip = "127.0.0.1";
    int port = 27017;
    int x = 1;

    if ( options ){
        printf( "can't handle options to mongo_connect yet" );
        exit(-2);
        return -2;
    }

    /* setup */
    conn->options = options;
    conn->sock = 0;

    memset( conn->sa.sin_zero , 0 , sizeof(conn->sa.sin_zero) );
    conn->sa.sin_family = AF_INET;
    conn->sa.sin_port = htons(port);
    conn->sa.sin_addr.s_addr = inet_addr( ip );
    conn->addressSize = sizeof(conn->sa);

    /* connect */
    conn->sock = socket( AF_INET, SOCK_STREAM, 0 );
    if ( conn->sock <= 0 ){
        fprintf( stderr , "couldn't get socket errno: %d" , errno );
        return -1;
    }

    if ( connect( conn->sock , &conn->sa , conn->addressSize ) ){
        fprintf( stderr , "couldn' connect errno: %d\n" , errno );
        return -2;
    }

    /* options */

    /* nagle */
    if ( setsockopt( conn->sock, IPPROTO_TCP, TCP_NODELAY, (char *) &x, sizeof(x) ) ){
        fprintf( stderr , "disbale nagle failed" );
        return -3;
    }

    /* TODO signals */

    return 0;
}

int mongo_insert( struct mongo_connection * conn , const char * ns , struct bson * bson ){
    char * data;
    struct mongo_message * mm = mongo_message_create( 16 + 4 + strlen( ns ) + 1 + bson_size( bson ) ,
                                                      0 , 0 , mongo_op_insert );
    if ( ! mm )
        return 0;

    data = &mm->data;
    memset( data , 0 , 4 );
    memcpy( data + 4 , ns , strlen( ns ) + 1 );
    memcpy( data + 4 + strlen( ns ) + 1 , bson->data , bson_size( bson ) );

    write( conn->sock , mm , mm->len );
    return 0;
}

char * mongo_data_append( char * start , const void * data , int len ){
    memcpy( start , data , len );
    return start + len;
}

struct mongo_message * mongo_read_response( struct mongo_connection * conn ){
    char smallbuf[16];
    struct mongo_message * mm;
    int total , temp;
    
    total = 0;
    while ( total < 16 ){
        temp = read( conn->sock , smallbuf , 16 - total );
        if ( temp < 0 ){
            fprintf( stderr , "error reading " );
            return 0;
        }
    }
}

void mongo_query( struct mongo_connection * conn , const char * ns , struct bson * query , struct bson * fields , int nToReturn , int nToSkip , int options ){
    char * data;
    struct mongo_message * mm = mongo_message_create( 16 + 
                                                      4 + /*  options */
                                                      strlen( ns ) + 1 + /* ns */
                                                      4 + 4 + /* skip,return */
                                                      bson_size( query ) +
                                                      bson_size( fields ) ,
                                                      0 , 0 , mongo_op_query );

    data = &mm->data;
    data = mongo_data_append( data , &options , 4 );
    data = mongo_data_append( data , ns , strlen( ns ) + 1 );    
    data = mongo_data_append( data , &nToSkip , 4 );
    data = mongo_data_append( data , &nToReturn , 4 );
    data = mongo_data_append( data , query->data , bson_size( query ) );    
    if ( fields )
        data = mongo_data_append( data , fields->data , bson_size( fields ) );    
    

    bson_fatal( "query building fail!" , data == ((char*)mm) + mm->len );

    write( conn->sock , mm , mm->len );
    
}

void mongo_exit_on_error( int ret ){
    if ( ret == 0 )
        return;

    printf( "unexpeted error: %d\n" , ret );
    exit(ret);
}
