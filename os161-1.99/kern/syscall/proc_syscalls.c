#include <types.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <kern/wait.h>
#include <lib.h>
#include <syscall.h>
#include <current.h>
#include <proc.h>
#include <thread.h>
#include <addrspace.h>
#include <copyinout.h>

// HELPERS
void removeChild(pid_t pid){
  for(int i = 0; i < curproc->family_size; i++){
    if(pid == curproc->family[i]->pid){
      free(curproc->family[i]); // Could be null?
      curproc->family[i] = curproc->family[curproc->family_size - 1];
      realloc(curproc->family, (curproc->family_size - 1) * sizeof(*curproc));
      return;
    }
  }
}

int getChildIndex(pid_t pid){
  for(int i = 0; i < curproc->family_size; i++){
    if(pid == curproc->family[i]->pid){
      return i;
    }
  }
  return -1;
}

bool hasExited(pid_t pid){
  for(int i = 0; i < curproc->family_size; i++){
    if(pid == curproc->family[i]->pid && curproc->family[i]->exit_status){ // Verify that exit status exists
      return true;
    }
  }
  return false;
}


// POTENTIAL ISSUES:
/* Pretty sure MKWAIT doesn't take in process as argument (nor an exit status is sufficient)
*
* sys_fork() -> May need to copy lock to child process
*  -> May need to handle ENPROC and EMPROC
*  -> May need to check spinlock placement again
*/

int sys_fork(struct trapframe *tf){

  int err;

  struct addrspace *new_addr;

  //Can NAMES BE THE SAME???? assume same name as parent
  struct proc *p = proc_create_runprogram(curproc->p_name);

  err = as_copy(curproc->p_addrspace, new_addr);
  if(err == ENOMEM){
    return ENOMEM;
    panic("Memory error");
  }

  // !!! Make sure that this spinlock encompasses enough
  spinlock_acquire(&p->p_lock);
	p->p_addrspace = new_addr;

  realloc(curproc->family, (curproc->family_size + 1) * sizeof(*curproc));
  curproc->family[curproc->family_size - 1] = p; // Add child process p to "family"
  curproc->family_size++;

  p->family_size = curproc->family_size;
  p->family = curproc->family; /* family is global */

	spinlock_release(&p->p_lock);

  // Make a copy of tf in the heap and pass it into enter_forked_process
  struct trapframe *childtf = kmalloc(sizeof(struct trapframe));
  memcpy(tf, childtf, sizeof *tf);

  err = thread_fork(curproc->p_name, curproc, enter_forked_process, childtf, 1);

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

  curproc->exitcode = exitcode;
  if(curproc->family){
    free(curproc->family);
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
  proc_destroy(p);
  
  thread_exit();
  /* thread_exit() does not return, so we should never get here */
  panic("return from thread_exit in sys_exit\n");
}


/* stub handler for getpid() system call                */
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

  /* Need to explore macro further for cv
  */
  lock_acquire(curproc->p_lock);
  while(!hasExited(pid)){
    cv_wait(curproc->pc_cv);
  }

  /* Once we know that the child process has exited, we get the exit_status */
  exitstatus = MKWAIT_EXIT(curproc->family[getChildIndex(pid)]->exitcode);

  // Remove the process from family array
  removeChild(pid);

  // We don't account for options in OS161
  if (options != 0) {
    lock->release(curproc->p_lock);
    return(EINVAL);
  }
  
  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) {
    lock->release(curproc->p_lock);
    return(result);
  }
  
  *retval = pid;
  lock->release(curproc->p_lock);
  return(0);
}