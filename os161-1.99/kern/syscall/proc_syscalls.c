#include <types.h>
#include <array.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <kern/wait.h>
#include <lib.h>
#include <synch.h>
#include <syscall.h>
#include <current.h>
#include <proc.h>
#include <thread.h>
#include <addrspace.h>
#include <copyinout.h>
#include <mips/trapframe.h>
#include "opt-A2.h"

// HELPERS
#if OPT_A2
/*static void krealloc_family(struct proc_info *family, size_t size, size_t currSize){
  //struct proc **new_family = kmalloc(size * sizeof(struct proc *));
  struct proc_info *new_family = kmalloc(size * sizeof(struct proc_info));
  if(currSize < size){
    for(size_t i = 0; i < currSize; i++){ // Copy over all existing elements (before nullspace)
      new_family[i] = family[i];
    }
  }
  else{
    for(size_t i = 0; i < currSize - 1; i++){ // Copy over all existing elements (before nullspace)
      new_family[i] = family[i];
    }
  }
  //possibly kfree family
  family = new_family;
}*/

static void removeChild(pid_t pid){
  for(unsigned i = 0; i < curproc->family->num; i++){
    if(pid == ((struct proc *) array_get(curproc->family, i))->pid){
      //kfree(curproc->family[i]); // Could be null?
      //curproc->family[i] = curproc->family[curproc->family_size - 1];
      //krealloc_family(curproc->family, curproc->family_size - 1, curproc->family_size);
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
#endif

// POTENTIAL ISSUES:
/* Pretty sure MKWAIT doesn't take in process as argument (nor an exit status is sufficient)
*
* sys_fork() -> May need to copy lock to child process
*  -> May need to handle ENPROC and EMPROC
*  -> May need to check spinlock placement again
*
* proc.c -> might need to acquire spinlock in proc_create_runprogram
*/

#if OPT_A2
int sys_fork(struct trapframe *tf){

  int err;

  struct proc *proc = curproc;
  struct addrspace *new_addr;

  //spinlock_acquire(&proc->p_lock);

  //Can NAMES BE THE SAME???? assume same name as parent
  struct proc *p = proc_create_runprogram(proc->p_name);

  // Make sure no p isn't modified until it is created
  //spinlock_acquire(&p->p_lock);

  if (p == NULL) {
		panic("WHY NULL????");
	}

  struct addrspace **dp_addr;
  err = as_copy(proc->p_addrspace, dp_addr);
  if(err == ENOMEM){
    return ENOMEM;
    panic("Memory error");
  }

  new_addr = *dp_addr;

  // !!! Make sure that this spinlock encompasses enough
	p->p_addrspace = new_addr;

  err = array_add(proc->family, p, NULL);
  if(err){
    panic("ExCuSe Me WtHeck");
  }

  p->family = proc->family;

  // Possibly need to free on create
  p->pc_lock = proc->pc_lock;
  p->pc_cv = proc->pc_cv;

  //spinlock_release(&p->p_lock);

  // Make a copy of tf in the heap and pass it into enter_forked_process
  struct trapframe *childtf = kmalloc(sizeof(*tf)); // Why differ?
  memcpy(tf, childtf, sizeof(*tf));

  err = thread_fork(proc->p_name, p, enter_forked_process, childtf, 1);

  if(err){
    panic("threadfork err lul");
  }
  //spinlock_release(&proc->p_lock);

  return 0;
}

  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */

void sys__exit(int exitcode) {

  // 1. Add exit code to "parameter"
  // 2. Deallocate family array if necessary (Basically always do it)

  struct addrspace *as;
  struct proc *p = curproc;

  DEBUG(DB_SYSCALL,"Syscall: _exit(%d)\n",exitcode);

  p->exitcode = exitcode;
  if(p->family){
    kfree(p->family);
  }

  KASSERT(p->p_addrspace != NULL);
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


int
sys_getpid(pid_t *retval)
{
  *retval = curproc->pid;
  return(0);
}

/* stub handler for waitpid() system call                */
/*
* pid - Child pid
* status - Child exit status (_WEXITED)
* options - Literally pointless (arg is always 0)
* retval - Argument passed into _exit() e.g 0 or 1
*
* Always returns pid or -1 if error
*/
int
sys_waitpid(pid_t pid,
	    userptr_t status,
	    int options,
	    pid_t *retval)
{
  int exitstatus;
  int result;

  struct proc *proc = curproc;

  /* Need to explore macro further for cv
  */
  lock_acquire(proc->pc_lock);
  while(!hasExited(pid)){
    cv_wait(proc->pc_cv, proc->pc_lock);
  }

  /* Once we know that the child process has exited, we get the exit_status */
  exitstatus = _MKWAIT_EXIT(((struct proc *) array_get(proc->family, getChildIndex(pid)))->exitcode);

  // Remove the process from family array
  removeChild(pid);

  // We don't account for options in OS161
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
#endif
