/* Bounded newlib heap. No firmware file/console IO uses libc syscalls. */
#include <sys/stat.h>
#include <errno.h>
#include <stddef.h>
extern char _heap_start, _heap_end;
void *_sbrk(ptrdiff_t increment) {
    static char *next=&_heap_start;
    if(increment<0 || increment>(&_heap_end-next)) { errno=ENOMEM; return (void *)-1; }
    char *old=next; next+=increment; return old;
}
int _write(int fd,const void *data,size_t count) { (void)fd; (void)data; (void)count; errno=EBADF; return -1; }
int _read(int fd,void *data,size_t count) { (void)fd; (void)data; (void)count; errno=EBADF; return -1; }
int _close(int fd) { (void)fd; errno=EBADF; return -1; }
int _fstat(int fd,struct stat *st) { (void)fd; st->st_mode=S_IFCHR; return 0; }
int _isatty(int fd) { (void)fd; return 0; }
int _lseek(int fd,int offset,int whence) { (void)fd; (void)offset; (void)whence; errno=EBADF; return -1; }
int _getpid(void) { return 1; }
int _kill(int pid,int sig) { (void)pid; (void)sig; errno=EINVAL; return -1; }
#ifdef USE_FREERTOS
#include "FreeRTOS.h"
#include "task.h"
struct _reent;
void __malloc_lock(struct _reent *r) { (void)r; vTaskSuspendAll(); }
void __malloc_unlock(struct _reent *r) { (void)r; (void)xTaskResumeAll(); }
#endif
