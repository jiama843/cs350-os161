#include <types.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <kern/wait.h>
#include <mips/trapframe.h>
#include <lib.h>
#include <synch.h>
#include <syscall.h>
#include <current.h>
#include <proc.h>
#include <thread.h>
#include <addrspace.h>
#include <copyinout.h>
#include "opt-A2.h"

#if OPT_A2

// Also handle setting hasParent to false
static void kill_family(struct array *family){
  while(family->num > 0){
    struct proc *child = (struct proc *) array_get(family, 0);
    if(child->exited){
      proc_destroy(child);
    }
    else{
      child->has_parent = false;
    }
    array_remove(family, 0);
  }
}

static void removeChild(struct array *family, pid_t pid){
  for(unsigned i = 0; i < array_num(family); ++i){
    struct proc *child = (struct proc *) array_get(family, i);
    if(pid == child->pid){
      proc_destroy(child);
      array_remove(family, i);
      return;
    }
  }
}

static int getChildIndex(struct array *family, pid_t pid){
  for(unsigned i = 0; i < array_num(family); ++i){
    struct proc *child = (struct proc *) array_get(family, i);
    if(pid == child->pid){
      return i;
    }
  }
  return -1;
}

static bool hasExited(struct array *family, pid_t pid){
  for(unsigned i = 0; i < array_num(family); ++i){
    struct proc *child = (struct proc *) array_get(family, i);
    if(pid == child->pid && child->exited){ // Verify that exit status exists
      return true;
    }
  }
  return false;
}

int sys_fork(struct trapframe *tf, pid_t *retval){

  int err;

  struct proc *proc = curproc;
 

  // Create child process
  struct proc *p = proc_create_runprogram(proc->p_name);
  if (p == NULL) {
    return ENOMEM;
	}


  // Create addrspace copy and add to child
  spinlock_acquire(&proc->p_lock);
  //struct addrspace *new_addr;
  //struct addrspace **dp_addr;

  err = as_copy(proc->p_addrspace, &p->p_addrspace);//dp_addr);
  if(err){
    return err;
  }

  //new_addr = *dp_addr;
	//p->p_addrspace = new_addr;
  spinlock_release(&proc->p_lock);


  // Make a copy of tf in the heap and pass it into enter_forked_process
  struct trapframe *childtf = kmalloc(sizeof(*tf));
  //memcpy(childtf, tf, sizeof(*tf));
  *childtf = *tf;

  err = thread_fork(proc->p_name, p, enter_forked_process, childtf, sizeof(*tf));
  if(err){
    return err;
  }


  // Set locks, cvs and parents between family
  p->pc_lock = proc->pc_lock;
  p->pc_cv = proc->pc_cv;
  p->parent = proc;
  p->has_parent = true;


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

// Actual removal of child processes is done in waitpid, unless parent doesn't exist
void sys__exit(int exitcode) {

  struct addrspace *as;
  struct proc *p = curproc;
  /* for now, just include this to keep the compiler from complaining about
     an unused variable */

  DEBUG(DB_SYSCALL,"Syscall: _exit(%d)\n",exitcode);

  // Set exit code for process
  p->exitcode = exitcode;
  p->exited = true;

  if(p->family->num > 0){
    kill_family(p->family);
    array_destroy(p->family);
    cv_broadcast(p->pc_cv, p->pc_lock);
  }
  else{
    cv_broadcast(p->pc_cv, p->pc_lock);
  }

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

  // Don't destroy child processes (to be done in parent by wait_pid)
  //USE a bool called has_parent
  if(!p->has_parent){
    proc_destroy(p);
  }
  
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
// Assume it can only be called from parent
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

  struct proc *proc = curproc;

  // Check if child process even exists otherwise return
  int child_index = getChildIndex(proc->family, pid);
  if(child_index == -1){
    return -1;
  }

  /* Wait on children if they haven't exited */
  lock_acquire(proc->pc_lock);
  while(!hasExited(proc->family, pid)){
    cv_wait(proc->pc_cv, proc->pc_lock);
  }

  /* Once we know that the child process has exited, we get the exit_status */
  exitstatus = _MKWAIT_EXIT(((struct proc *) array_get(proc->family, child_index))->exitcode);

  // Remove the process from family array
  removeChild(proc->family, pid);

  if (options != 0) {
    lock_release(proc->pc_lock);
    return(EINVAL);
  }

  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) {
    lock_release(proc->pc_lock);
    return(result);
  }

  *retval = pid;
  lock_release(proc->pc_lock);
  return(0);
}

