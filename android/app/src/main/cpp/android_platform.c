#define _GNU_SOURCE
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include <sys/system_properties.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include "platform.h"
#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0x100000
#endif
#ifndef MAP_NORESERVE
#define MAP_NORESERVE 0
#endif
static pthread_mutex_t g_lock=PTHREAD_MUTEX_INITIALIZER;
static uint64_t g_lo,g_hi;
static uint64_t page_size(void){long p=sysconf(_SC_PAGESIZE);return p>0?(uint64_t)p:4096;}
static bool aligned_page(uint64_t x){return x%page_size()==0;}
static bool valid_range(uint64_t lo,uint64_t hi){return lo<hi&&aligned_page(lo)&&aligned_page(hi);}
static bool inside_guest(uint64_t addr,uint64_t size){return g_lo&&size&&addr>=g_lo&&addr<=g_hi&&size<=g_hi-addr;}
bool plat_reserve_guest(uint64_t lo,uint64_t hi){
 if(!valid_range(lo,hi))return false;
 pthread_mutex_lock(&g_lock);
 if(g_lo||g_hi){pthread_mutex_unlock(&g_lock);return false;}
 void *want=(void*)(uintptr_t)lo;
 void *p=mmap(want,(size_t)(hi-lo),PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_NORESERVE|MAP_FIXED_NOREPLACE,-1,0);
 bool ok=p==want;
 if(p!=MAP_FAILED&&p!=want)munmap(p,(size_t)(hi-lo));
 if(ok){g_lo=lo;g_hi=hi;}
 pthread_mutex_unlock(&g_lock);
 return ok;
}
void plat_commit(uint64_t addr,uint64_t size){
 pthread_mutex_lock(&g_lock);
 bool valid=inside_guest(addr,size)&&aligned_page(addr)&&aligned_page(size);
 pthread_mutex_unlock(&g_lock);
 (void)valid; // The complete reservation is already backed by a private anonymous mapping.
}
bool plat_reset_guest(uint64_t lo,uint64_t hi){
 if(!valid_range(lo,hi))return false;
 pthread_mutex_lock(&g_lock);
 bool inside=g_lo&&lo>=g_lo&&hi<=g_hi;
 if(!inside){pthread_mutex_unlock(&g_lock);return false;}
 void *want=(void*)(uintptr_t)lo;
 void *p=mmap(want,(size_t)(hi-lo),PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_NORESERVE|MAP_FIXED,-1,0);
 bool ok=p==want;
 pthread_mutex_unlock(&g_lock);
 return ok;
}
int plat_written_pages(uint64_t a,uint64_t s,uint64_t*p,int m,uint64_t*pg){(void)a;(void)s;(void)p;(void)m;(void)pg;return -1;}
int plat_written_pages_peek(uint64_t a,uint64_t s){(void)a;(void)s;return -1;}
bool plat_page_state(uint64_t a,char*o,size_t n){(void)a;(void)o;(void)n;return false;}
bool plat_mem_usage(uint64_t*p,uint64_t*g){(void)p;(void)g;return false;}
void plat_sample_focus(void){}
int plat_protect(uint64_t a,uint64_t s,int prot){
 int p;
 switch(prot){case PLAT_PROT_NONE:p=PROT_NONE;break;case PLAT_PROT_READ:p=PROT_READ;break;case PLAT_PROT_RW:p=PROT_READ|PROT_WRITE;break;default:errno=EINVAL;return -1;}
 if(!s||!aligned_page(a)||!aligned_page(s)){errno=EINVAL;return -1;}
 pthread_mutex_lock(&g_lock);
 bool inside=inside_guest(a,s);
 int result=inside?mprotect((void*)(uintptr_t)a,(size_t)s,p):-1;
 pthread_mutex_unlock(&g_lock);
 if(!inside)errno=EINVAL;
 return result;
}
uint64_t plat_now_ns(void){struct timespec t;if(clock_gettime(CLOCK_MONOTONIC,&t))return 0;return(uint64_t)t.tv_sec*1000000000ull+(uint64_t)t.tv_nsec;}
uint64_t plat_thread_cpu_ns(void){struct timespec t;if(clock_gettime(CLOCK_THREAD_CPUTIME_ID,&t))return 0;return(uint64_t)t.tv_sec*1000000000ull+(uint64_t)t.tv_nsec;}
void plat_sleep_until_ns(uint64_t n){struct timespec t={(time_t)(n/1000000000ull),(long)(n%1000000000ull)};while(clock_nanosleep(CLOCK_MONOTONIC,TIMER_ABSTIME,&t,0)==EINTR){}}
int plat_cpu_count(void){cpu_set_t s;if(sched_getaffinity(0,sizeof(s),&s))return 0;int n=0;for(int i=0;i<CPU_SETSIZE;i++)if(CPU_ISSET(i,&s))n++;return n;}
void plat_thread_guest(int p){(void)p;}
void plat_thread_realtime(uint64_t p){(void)p;}
int plat_thread_realtime_check(uint64_t p){(void)p;return -1;}

void plat_init(void) {
    // Android process and storage setup is owned by the Java Activity layer.
}

void plat_system_info(PlatSysInfo *si) {
    if (!si) return;
    struct utsname u;
    memset(&u, 0, sizeof u);
    const bool have_uname = uname(&u) == 0;

    char release[PROP_VALUE_MAX] = {0};
    if (__system_property_get("ro.build.version.release", release) <= 0)
        snprintf(release, sizeof release, "unknown");

    snprintf(si->os, sizeof si->os, "Android %s (%s, kernel %.32s)",
             release, have_uname ? u.machine : "arm64",
             have_uname ? u.release : "unknown");
    snprintf(si->cpu, sizeof si->cpu, "%s", have_uname ? u.machine : "ARM64");

    const long count = sysconf(_SC_NPROCESSORS_ONLN);
    if (count > 0 && count <= INT32_MAX) {
        si->cores = (int)count;   // Physical core topology is not reliably exposed to an app.
        si->threads = (int)count;
    }
    struct sysinfo memory;
    if (sysinfo(&memory) == 0)
        si->ram = (uint64_t)memory.totalram * (uint64_t)memory.mem_unit;
    si->package = NULL;
}

bool plat_exe_path(char *buf, size_t size) {
    // /proc/self/exe resolves to Android's app_process binary, not this APK's asset/resource directory.
    // Returning false prevents the desktop resource-path logic from treating it as an application bundle.
    if (buf && size) buf[0] = 0;
    return false;
}

void plat_os_name(char *buf, size_t size) {
    if (!buf || !size) return;
    char release[PROP_VALUE_MAX] = {0};
    if (__system_property_get("ro.build.version.release", release) <= 0)
        snprintf(release, sizeof release, "Android");
    struct utsname u;
    if (uname(&u) == 0)
        snprintf(buf, size, "Android %s (kernel %s)", release, u.release);
    else
        snprintf(buf, size, "Android %s", release);
}
