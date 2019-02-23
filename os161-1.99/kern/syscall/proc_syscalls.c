#include <types.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <kern/wait.h>
#include <mips/trapframe.h>
#include <lib.h>
#include <syscall.h>
#include <current.h>
#include <proc.h>
#include <thread.h>
#include <addrspace.h>
#include <copyinout.h>
#include "opt-A2.h"

#if OPT_A2
static void removeChild(pid_t pid){
  for(unsigned i = 0; i < curproc->family->num; i++){
    if(pid == ((struct proc *) array_get(curproc->family, i))->pid){
      array_remove(curproc->family, i);
      return;
    }
  }
}

static int getChildIndex(pid_t pid){
  for(unsigned i = 0; i < curproc->family->num; i++){
    if(pid == ((struct proc *) array_get(curproc->family, i))->pid){
      return i;
    }
  }
  return -1;
}

static bool hasExited(pid_t pid){
  for(unsigned i = 0; i < curproc->family->num; i++){
    if(pid == ((struct proc *) array_get(curproc->family, i))->pid && 
        ((struct proc *) array_get(curproc->family, i))->exitcode){ // Verify that exit status exists
      return true;
    }
  }
  return false;
}

int sys_fork(struct trapframe *tf, pid_t *retval){

  int err;

  struct proc *proc = curproc;
  struct addrspace *new_addr;

  // Create child process
  struct proc *p = proc_create_runprogram(proc->p_name);
  if (p == NULL) {
    return ENOMEM;
	}


  // Create addrspace copy and add to child
  spinlock_acquire(&proc->p_lock);
  struct addrspace **dp_addr;

  err = as_copy(proc->p_addrspace, dp_addr);
  if(err){
    return err;
  }

  new_addr = *dp_addr;
	p->p_addrspace = new_addr;
  spinlock_release(&proc->p_lock);


  // Make a copy of tf in the heap and pass it into enter_forked_process
  struct trapframe *childtf = kmalloc(sizeof(*tf));
  //memcpy(childtf, tf, sizeof(*tf));
  *childtf = *tf;

  err = thread_fork(proc->p_name, p, enter_forked_process, childtf, sizeof(*tf));
  if(err){
    return err;
  }


  // Return PID
  *retval = p->pid;


  // Add process to family
  err = array_add(proc->family, p, NULL);
  if(err){
    return err;
  }

  return 0;
}
#endif


  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */

void sys__exit(int exitcode) {

  struct addrspace *as;
  struct proc *p = curproc;
  /* for now, just include this to keep the compiler from complaining about
     an unused variable */
  (void)exitcode;

  DEBUG(DB_SYSCALL,"Syscall: _exit(%d)\n",exitcode);

  KASSERT(curproc->p_addrspace != NULL);
  as_deactivate();
  /*
   * clear p_addrspace before calling as_destroy. Otherwise if
   * as_destroy sleeps (which is quite possible) when we
   * come back we'll be calling as_activate on a
   * half-destroyed address space. This tends to be
   * messily fatal.
   */
  as = curproc_setas(NULL);
  as_destroy(as);

  /* detach this thread from its process */
  /* note: curproc cannot be used after this call */
  proc_remthread(curthread);

  /* if this is the last user process in the system, proc_destroy()
     will wake up the kernel menu thread */
  proc_destroy(p);
  
  thread_exit();
  /* thread_exit() does not return, so we should never get here */
  panic("return from thread_exit in sys_exit\n");
}


/* stub handler for getpid() system call                */
int
sys_getpid(pid_t *retval)
{
  /* for now, this is just a stub that always returns a PID of 1 */
  /* you need to fix this to make it work properly */
  #if OPT_A2
  *retval = curproc->pid;
  #else
  *retval = 1;
  #endif

  return(0);
}

/* stub handler for waitpid() system call                */

int
sys_waitpid(pid_t pid,
	    userptr_t status,
	    int options,
	    pid_t *retval)
{
  int exitstatus;
  int result;

  /* this is just a stub implementation that always reports an
     exit status of 0, regardless of the actual exit status of
     the specified process.   
     In fact, this will return 0 even if the specified process
     is still running, and even if it never existed in the first place.

     Fix this!
  */

  if (options != 0) {
    return(EINVAL);
  }
  /* for now, just pretend the exitstatus is 0 */
  exitstatus = 0;
  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) {
    return(result);
  }
  *retval = pid;
  return(0);
}

