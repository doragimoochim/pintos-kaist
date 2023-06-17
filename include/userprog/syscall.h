#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
/*pro2 - 추가*/
#include "threads/synch.h"
/*여기까지*/

void syscall_init (void);
/*pro2 - 추가 */
struct lock filesys_lock;
/*여기까지*/

#endif /* userprog/syscall.h */
