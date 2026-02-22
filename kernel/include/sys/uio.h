#pragma once

#include <sys/types.h>

/*
 * Forward declarations
 */

typedef struct thread thread_t;

/*
 * Buffer for vectored IO
 */

typedef struct uio_op_buf
{
    void   *base;
    size_t  length;
}
uio_op_buf_t;

/*
 * User IO operation type
 */

typedef enum uio_op_type
{
    UIO_OP_READ,
    UIO_OP_WRITE
}
uio_op_type_t;

/*
 * User IO operation
 */

typedef struct uio_op
{
    uio_op_type_t  type;        // read/write
	uio_op_buf_t  *buf; 	    // gather/scatter list
	size_t		   buf_cnt;	    // length of gather/scatter list
	off_t		   offset;	    // offset in target object
	ssize_t		   rem_bytes;   // remaining bytes to process
	thread_t      *thread;      // owner
}
uio_op_t;
