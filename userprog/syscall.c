#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "userprog/process.h"
#include "threads/flags.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "intrinsic.h"
#include "threads/synch.h"
#include "devices/input.h"
#include "lib/kernel/stdio.h"
#include "threads/palloc.h"
#include "lib/stdio.h"
#include "vm/vm.h"

void syscall_entry(void);
void syscall_handler(struct intr_frame *);
void check_address(void *addr);
void halt(void);
void exit(int status);
bool create(const char *file, unsigned initial_size);
bool remove(const char *file);
int open(const char *file_name);
int filesize(int fd);
int read(int fd, void *buffer, unsigned size);
int write(int fd, const void *buffer, unsigned size);
void seek(int fd, unsigned position);
unsigned tell(int fd);
void close(int fd);
tid_t fork(const char *thread_name, struct intr_frame *f);
int exec(const char *cmd_line);
int wait(int pid);	//선언
/*pro 3 추가*/
void *mmap(void *addr, size_t length, int writable, int fd, off_t offset);
void munmap(void *addr);
/*여기까지*/

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

void syscall_init(void)
{
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48 |
							((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t)syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
	/*pro2-추가*/
	lock_init(&filesys_lock);
	/*여기까지*/
}

/* The main system call interface */
void syscall_handler(struct intr_frame *f UNUSED)
{
	// TODO: Your implementation goes here.
	/* pro-2 추가 */
	int syscall_n = f->R.rax; /* 시스템 콜 넘버 */
	/*pro3 추가*/
#ifdef VM
	thread_current()->rsp = f->rsp;
#endif
/*여기까지*/
	switch (syscall_n)
	{
	case SYS_HALT:
		halt();
		break;
	case SYS_EXIT:
		exit(f->R.rdi);
		break;
	case SYS_FORK:
		f->R.rax = fork(f->R.rdi, f);
		break;
	case SYS_EXEC:
		f->R.rax = exec(f->R.rdi);
		break;
	case SYS_WAIT:
		f->R.rax = wait(f->R.rdi);
		break;
	case SYS_CREATE:
		f->R.rax = create(f->R.rdi, f->R.rsi);
		break;
	case SYS_REMOVE:
		f->R.rax = remove(f->R.rdi);
		break;
	case SYS_OPEN:
		f->R.rax = open(f->R.rdi);
		break;
	case SYS_FILESIZE:
		f->R.rax = filesize(f->R.rdi);
		break;
	case SYS_READ:
		f->R.rax = read(f->R.rdi, f->R.rsi, f->R.rdx);
		break;
	case SYS_WRITE:
		f->R.rax = write(f->R.rdi, f->R.rsi, f->R.rdx);
		break;
	case SYS_SEEK:
		seek(f->R.rdi, f->R.rsi);
		break;
	case SYS_TELL:
		f->R.rax = tell(f->R.rdi);
		break;
	case SYS_CLOSE:
		close(f->R.rdi);
		break; //교수님 말씀
	/*pro3 추가*/
	case SYS_MMAP:
		f->R.rax = mmap(f->R.rdi, f->R.rsi, f->R.rdx, f->R.r10, f->R.r8);
		break;
	case SYS_MUNMAP:
		munmap(f->R.rdi);
		break;
	} //여기까지

	// printf ("system call!\n");
	// thread_exit ();
}

/* pro2 - 추가 
 * addr을 검사하여 유효한 사용자 주소인지 확인
 */
void check_address(void *addr)
{
	if (addr == NULL)
		exit(-1);
	if (!is_user_vaddr(addr))
		exit(-1);
	/* 함수 사용하여 addr에 대한 페이지를 가져옴
	 * 페이지가 존재하지 않는 경우(NULL반환) → 프로세스 종료
	 */
	// if (pml4_get_page(thread_current()->pml4, addr) == NULL)
	// 	exit(-1);
}

/* pro-2 추가 */
/* 핀토스 종료.
 * 교착 상태에 빠질 수 있는 상황 등에 대한 일부 정보를 잃게 되므로
 * 거의 사용하지 않는 것이 좋다
 */
void halt(void)
{
	power_off();
}

/* 0 status 성공, 0이 아닌 값 오류
 * 현재 사용자 프로그램을 종료하여 상태를 커널로 되돌림.
 */
void exit(int status)
{
	struct thread *curr = thread_current();
	curr->exit_status = status;
	printf("%s: exit(%d)\n", curr->name, status);
	thread_exit();
}

bool create(const char *file, unsigned initial_size)
{
	/*pro3 추가*/
	lock_acquire(&filesys_lock);
	/*여기까지*/
	check_address(file);
	/*pro3추가*/
	bool success = filesys_create(file, initial_size);
	lock_release(&filesys_lock);
	return success;
	/*여기까지*/
	// return filesys_create(file, initial_size);
}

bool remove(const char *file)
{
	check_address(file);
	return filesys_remove(file);
}

int open(const char *file_name)
{
	check_address(file_name);
	/*pro3 추가*/
	lock_acquire(&filesys_lock);
	struct file *file = filesys_open(file_name);
	if (file == NULL)
	{
		lock_release(&filesys_lock);
		return -1;
	}	
	int fd = process_add_file(file);
	if (fd == -1)
		file_close(file);
	lock_release(&filesys_lock);
	/*여기까지*/
	return fd;
}

int filesize(int fd)
{
	struct file *file = process_get_file(fd);
	if (file == NULL)
		return -1;
	return file_length(file);
}

void seek(int fd, unsigned position)
{
	struct file *file = process_get_file(fd);
	if (file == NULL)
		return;
	file_seek(file, position);
}

unsigned tell(int fd)
{
	struct file *file = process_get_file(fd);
	if (file == NULL)
		return;
	return file_tell(file);
}

void close(int fd)
{
	struct file *file = process_get_file(fd);
	if (file == NULL)
		return;
	file_close(file);
	process_close_file(fd);
}
int read(int fd, void *buffer, unsigned size)
{
	check_address(buffer);

	char *ptr = (char *)buffer;
	int bytes_read = 0;

	lock_acquire(&filesys_lock);
	if (fd == STDIN_FILENO)
	{
		for (int i = 0; i < size; i++)
		{
			*ptr++ = input_getc();
			bytes_read++;
		}
		lock_release(&filesys_lock);
	}
	else
	{
		if (fd < 2)
		{

			lock_release(&filesys_lock);
			return -1;
		}
		struct file *file = process_get_file(fd);
		if (file == NULL)
		{

			lock_release(&filesys_lock);
			return -1;
		}
		/*pro3 추가*/
		struct page *page = spt_find_page(&thread_current()->spt, buffer);
		if (page && !page->writable)
		{
			lock_release(&filesys_lock);
			exit(-1);

		}
		/*여기까지*/
		bytes_read = file_read(file, buffer, size);
		lock_release(&filesys_lock);
	}
	return bytes_read;
}

int write(int fd, const void *buffer, unsigned size)
{
	check_address(buffer);
	int bytes_write = 0;
	if (fd == STDOUT_FILENO)
	{
		putbuf(buffer, size);
		bytes_write = size;
	}
	else
	{
		if (fd < 2)
			return -1;
		struct file *file = process_get_file(fd);
		if (file == NULL)
			return -1;
		lock_acquire(&filesys_lock);
		bytes_write = file_write(file, buffer, size);
		lock_release(&filesys_lock);
	}
	return bytes_write;
}

tid_t fork(const char *thread_name, struct intr_frame *f)
{
	return process_fork(thread_name, f);
}
/* cmd_line : 실행할 명령어 문자열
 * 주어진 명령어를 실행하기 위해 필요한 작업 수행
 * 프로세스의 실행 흐름을 변경하는 중요한 역할 */
int exec(const char *cmd_line)
{
	/* 주소의 유효성을 검사하는 목적 : 프로그램이 잘못된 주소에 접근하려는 시도방지
	 * 세그멘테이션 오류와 같은 예기지 않은 동작 방지*/
	check_address(cmd_line);

	//process.c 파일의 process_initd 함수와 유사하다
	//단, 스레드를 새로생성하는 건 fork에서 수행하므로
	//이 함수에서는 새 스레드를 생성하지 않고 process_exec을 호출
	
	//process_exec함수 안에서 filename을 변경해야 하므로
	//커널 메모리 공간에 cmd_line의 복사본을 만든다.
	//(현재는 const char*형식이기 때문에 수정불가)
	char *cmd_line_copy;
	/* palloc_get_page함수 사용 페이지 크기의 메모리 할당
	 * cmd_line_copy라는 포인터 사용
	 * cmd_line_copy가 할당된 메모리 페이지를 가리키도록 함 */
	cmd_line_copy = palloc_get_page(0);
	if (cmd_line_copy == NULL)
		exit(-1);								//메모리 할당 실패 시 status -1로 종료
	strlcpy(cmd_line_copy, cmd_line, PGSIZE);	//cmd_line을 복사

	//스레드의 이름을 변경하지 않고 바로 실행
	/* process_exec함수 호출 → cmd_line_copy 를 실행
	   → 주어진 명령어를 새로운 프로세스로 실행하는 역할 */
	if (process_exec(cmd_line_copy) == -1)
		exit(-1);	//실패 시 status -1로 종료
	
	//이 함수는 성공한 경우 반환하지 않으므로 별도의 return은 없다.
	//여기까지
}
/*pro2- 추가*/
int wait(int pid)
{
	return process_wait(pid);
}

void *mmap(void *addr, size_t length, int writable, int fd, off_t offset)
{
	if (!addr || addr != pg_round_down(addr))
		return NULL;
	
	if (offset != pg_round_down(offset))
		return NULL;
	
	if (!is_user_vaddr(addr) || !is_user_vaddr(addr + length))
		return NULL;
	
	if (spt_find_page(&thread_current()->spt, addr))
		return NULL;
	
	struct file *f = process_get_file(fd);
	if (f == NULL)
		return NULL;

	if (file_length(f) == 0 || (int)length <= 0)
		return NULL;

	return do_mmap(addr, length, writable, f, offset);	//파일이 매핑된 가상 주소 반환
}

void munmap(void *addr)
{
	do_munmap(addr);
}