#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include <stdbool.h>
#include "threads/interrupt.h"
#ifdef VM
#include "vm/vm.h"
#endif


/* States in a thread's life cycle. */
enum thread_status {
	THREAD_RUNNING,     /* Running thread. */
	THREAD_READY,       /* Not running but ready to run. */
	THREAD_BLOCKED,     /* Waiting for an event to trigger. */
	THREAD_DYING        /* About to be destroyed. */
};

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t) -1)          /* Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0                       /* Lowest priority. */
#define PRI_DEFAULT 31                  /* Default priority. */
#define PRI_MAX 63                      /* Highest priority. */

#define true 1
#define false 0

/* A kernel thread or user process.
 *
 * Each thread structure is stored in its own 4 kB page.  The
 * thread structure itself sits at the very bottom of the page
 * (at offset 0).  The rest of the page is reserved for the
 * thread's kernel stack, which grows downward from the top of
 * the page (at offset 4 kB).  Here's an illustration:
 *
 *      4 kB +---------------------------------+
 *           |          kernel stack           |
 *           |                |                |
 *           |                |                |
 *           |                V                |
 *           |         grows downward          |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           +---------------------------------+
 *           |              magic              |
 *           |            intr_frame           |
 *           |                :                |
 *           |                :                |
 *           |               name              |
 *           |              status             |
 *      0 kB +---------------------------------+
 *
 * The upshot of this is twofold:
 *
 *    1. First, `struct thread' must not be allowed to grow too
 *       big.  If it does, then there will not be enough room for
 *       the kernel stack.  Our base `struct thread' is only a
 *       few bytes in size.  It probably should stay well under 1
 *       kB.
 *
 *    2. Second, kernel stacks must not be allowed to grow too
 *       large.  If a stack overflows, it will corrupt the thread
 *       state.  Thus, kernel functions should not allocate large
 *       structures or arrays as non-static local variables.  Use
 *       dynamic allocation with malloc() or palloc_get_page()
 *       instead.
 *
 * The first symptom of either of these problems will probably be
 * an assertion failure in thread_current(), which checks that
 * the `magic' member of the running thread's `struct thread' is
 * set to THREAD_MAGIC.  Stack overflow will normally change this
 * value, triggering the assertion. */
/* The `elem' member has a dual purpose.  It can be an element in
 * the run queue (thread.c), or it can be an element in a
 * semaphore wait list (synch.c).  It can be used these two ways
 * only because they are mutually exclusive: only a thread in the
 * ready state is on the run queue, whereas only a thread in the
 * blocked state is on a semaphore wait list. */
struct thread {
	/* Owned by thread.c. */
	tid_t tid;                          /* Thread identifier. */
	enum thread_status status;          /* Thread state. */
	char name[16];                      /* Name (for debugging purposes). */
	int priority;                       /* Priority. */

	/*추가*/
	int init_priority;	//thread의 priority는 donation에 의해 매번 바뀔 수 있음. 그러니 맨 처음에 할당받은 priority를 기억해둬야 함.
	struct lock *wait_on_lock;	//해당 스레드가 대기하고 있는 lock자료구조 주소 저장: thread가 원하는 lock을 이미 다른 thread가 점유하고 있으면 lock의 주소 저장
	struct list donations;	//multiple donation 고려하기 위해 사용 : A thread가 B thread에 의해 priority가 변경됐다면 A thread의 list donations에 B 스레드를 기억해놓는다.
	struct list_elem donation_elem;	// multiple donation 고려하기 위해 사용 : B thread는 A thread의 기부자 목록에 자신 이름 새겨놓아야! 이를 donation_elem
	/*여기까지*/

	/* Shared between thread.c and synch.c. */
	struct list_elem elem;              /* List element. */
	/* 추가코드 */
	int64_t wakeup_tick;
	/* Timer tick this thread would be woke up //여기까지 */

#ifdef USERPROG
	/* Owned by userprog/process.c. */
	uint64_t *pml4;                     /* Page map level 4 */
#endif
#ifdef VM
	/* Table for whole virtual memory owned by thread. */
	struct supplemental_page_table spt;
#endif

	/* Owned by thread.c. */
	struct intr_frame tf;               /* Information for switching */
	unsigned magic;                     /* Detects stack overflow. */
};

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

/* 추가코드 */
void thread_sleep(int64_t ticks);	//실행중인 스레드를 슬립으로 재운다.
void thread_awake(int64_t ticks);	//슬립 큐에서 깨워야 할 스레드를 깨운다.
void update_next_tick_to_awake(int64_t ticks);	//최소 틱을 가진 스레드를 저장한다.
int64_t get_next_tick_to_awake(void);	//thread.c의 next_tick_to_awake 반환
/* 여기까지 */

/* 추가함수 우선순위 */
void
test_max_priority (void);
/* 현재 수행중인 스레드와 가장 높은 우선순위의 스레드의 우선순위를 비교하여 스케줄링 */

bool 
cmp_priority (const struct list_elem *a,
				const struct list_elem *b,
				void *aux UNUSED);
		/* 인자로 주어진 스레드들의 우선순위를 비교 */

/*여기까지*/

/* 추가 */
bool
cmp_sem_priority (const struct list_elem *a,
					const struct list_elem *b,
					void *aux);
/* 여기까지 */

void thread_init (void);
void thread_start (void);

void thread_tick (void);
void thread_print_stats (void);

typedef void thread_func (void *aux);
tid_t thread_create (const char *name, int priority, thread_func *, void *);

void thread_block (void);
void thread_unblock (struct thread *);

struct thread *thread_current (void);
tid_t thread_tid (void);
const char *thread_name (void);

void thread_exit (void) NO_RETURN;
void thread_yield (void);

int thread_get_priority (void);
void thread_set_priority (int);

int thread_get_nice (void);
void thread_set_nice (int);
int thread_get_recent_cpu (void);
int thread_get_load_avg (void);

void do_iret (struct intr_frame *tf);
bool
thread_compare_donate_priority (const struct list_elem *l,
								const struct list_elem *s, void *aux UNUSED);

// /* 코드추가 */
// /* wakeup_tick comaprator for keeping sleep_list non-descending order */
// bool thread_wakeup_tick_compare(const struct list_elem *a,

// const struct list_elem *b,

// void *aux);
// void thread_sleep(int64_t);
// void thread_wakeup(int64_t);
// /*여기까지*/



#endif /* threads/thread.h */
