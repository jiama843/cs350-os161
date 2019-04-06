/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

//#include <synch.h>
#include <types.h>
#include <limits.h>
#include <kern/errno.h>
#include <lib.h>
#include <spl.h>
#include <spinlock.h>
#include <proc.h>
#include <current.h>
#include <mips/tlb.h>
#include <addrspace.h>
#include <copyinout.h>
#include <vm.h>

/*
 * Dumb MIPS-only "VM system" that is intended to only be just barely
 * enough to struggle off the ground.
 */

/* under dumbvm, always have 48k of user stack */
#define DUMBVM_STACKPAGES    12

/*
 * Wrap rma_stealmem in a spinlock.
 */
static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;

static struct spinlock coremap_lock = SPINLOCK_INITIALIZER;
//static struct lock *coremap_lock;

static struct coremap* coremap; /* Store coremap */

void
vm_bootstrap(void)
{

	paddr_t firstaddr, lastaddr;
	ram_getsize(&firstaddr, &lastaddr);

	int num_coremap_frames = (lastaddr - firstaddr)/ 1024 / PAGE_SIZE;

	coremap = (struct coremap *) PADDR_TO_KVADDR(firstaddr);

	coremap->firstaddr = ROUNDUP(firstaddr + num_coremap_frames * sizeof(int) + PAGE_SIZE, PAGE_SIZE); // Load coremap into first page and 

	coremap->lastaddr = lastaddr;
	coremap->total_frames = (coremap->lastaddr - coremap->firstaddr) / PAGE_SIZE;

	coremap->map = (int *) PADDR_TO_KVADDR(firstaddr + sizeof(paddr_t) * 5);
	for(int i = 0; i < coremap->total_frames; i++){
		coremap->map[i] = 0;
	}

	coremap->allocated = true;
}

static
paddr_t
getppages(unsigned long npages)
{
	if(coremap != NULL && coremap->allocated){

		spinlock_acquire(&coremap_lock);
		for(int i = 0; i < coremap->total_frames; i++){

			// Check if there are npage contiguous frames available
			bool can_alloc = true;
			for(size_t seg_page = 0; seg_page < npages; seg_page++){
				if(coremap->map[i + seg_page] != 0){
					can_alloc = false;
					break;
				}
			}

			if(!can_alloc){
				continue;
			}

			// if can_alloc, we alloc
			for(size_t seg_page = 0; seg_page < npages; seg_page++){
				coremap->map[i + seg_page] = seg_page + 1;
			}

			/*for(int j = 0; j < coremap->total_frames; j++){
				kprintf("%d ", coremap->map[j]);
			}
			kprintf("\n");*/

			spinlock_release(&coremap_lock);
			return (paddr_t) (coremap->firstaddr + i * PAGE_SIZE);
		}

		spinlock_release(&coremap_lock);
		
		return 0;
	}
	else{
		paddr_t addr;

		spinlock_acquire(&stealmem_lock);

		addr = ram_stealmem(npages);
		
		spinlock_release(&stealmem_lock);
		return addr;
	}
}

/* Allocate/free some kernel-space virtual pages */
vaddr_t 
alloc_kpages(int npages)
{

	// Check to see if using coremap (if so, never call getppages again)
	if(coremap != NULL && coremap->allocated){

		spinlock_acquire(&coremap_lock);

		for(int i = 0; i < coremap->total_frames; i++){

			// Check if there are npage contiguous frames available
			bool can_alloc = true;
			for(int seg_page = 0; seg_page < npages; seg_page++){
				if(coremap->map[i + seg_page] != 0){
					can_alloc = false;
					break;
				}
			}

			if(!can_alloc){
				continue;
			}

			// if can_alloc, we alloc
			/*for(int seg_page = 0; seg_page < npages; seg_page++){
				coremap->map[i + seg_page] = seg_page + 1;
			}*/

			spinlock_release(&coremap_lock);

			return PADDR_TO_KVADDR((paddr_t) (coremap->firstaddr + i * PAGE_SIZE));
		}

		spinlock_release(&coremap_lock);

		return 0; // Should return out of memory error
	}
	else{
		paddr_t pa;
		pa = getppages(npages);
		if (pa==0) {
			return 0;
		}
		return PADDR_TO_KVADDR(pa);
	}
}

void 
free_kpages(vaddr_t addr)
{

	spinlock_acquire(&coremap_lock);

	int frame = ((addr - 0x80000000) - coremap->firstaddr) / PAGE_SIZE; // Translate to paddr first

	// Clear coremap
	int i = frame;
	if(coremap->map[i] != 1){
		kprintf("Cannot deallocate in the middle of a block\n");
	}
	i++;

	while(coremap->map[i] == coremap->map[i - 1] + 1){
		i++;
	}

	for(int f = frame; f < i; f++){
		coremap->map[f] = 0;
	}

	for(int j = 0; j < coremap->total_frames; j++){
		kprintf("%d ", coremap->map[j]);
	}
	kprintf("\n");

	//lock_release(coremap_lock);
	spinlock_release(&coremap_lock);
}

void
vm_tlbshootdown_all(void)
{
	panic("dumbvm tried to do tlb shootdown?!\n");
}

void
vm_tlbshootdown(const struct tlbshootdown *ts)
{
	(void)ts;
	panic("dumbvm tried to do tlb shootdown?!\n");
}

int
vm_fault(int faulttype, vaddr_t faultaddress)
{
	vaddr_t vbase1, vtop1, vbase2, vtop2, stackbase, stacktop;
	paddr_t paddr;
	int i;
	uint32_t ehi, elo;
	struct addrspace *as;
	int spl;

	faultaddress &= PAGE_FRAME;

	DEBUG(DB_VM, "dumbvm: fault: 0x%x\n", faultaddress);

	switch (faulttype) {
		case VM_FAULT_READONLY:
		/* We always create pages read-write, so we can't get this */
		//panic("dumbvm: got VM_FAULT_READONLY\n");
			return 6;
		case VM_FAULT_READ:
		case VM_FAULT_WRITE:
			break;
	  default:
			return EINVAL;
	}

	if (curproc == NULL) {
		/*
		 * No process. This is probably a kernel fault early
		 * in boot. Return EFAULT so as to panic instead of
		 * getting into an infinite faulting loop.
		 */
		return EFAULT;
	}

	as = curproc_getas();
	if (as == NULL) {
		/*
		 * No address space set up. This is probably also a
		 * kernel fault early in boot.
		 */
		return EFAULT;
	}

	/* Assert that the address space has been set up properly. */
	KASSERT(as->as_vbase1 != 0);
	KASSERT(as->as_ptable1 != NULL);
	KASSERT(as->as_npages1 != 0);
	KASSERT(as->as_vbase2 != 0);
	KASSERT(as->as_ptable2 != NULL);
	KASSERT(as->as_npages2 != 0);
	KASSERT(as->as_pstacktable != NULL);
	KASSERT((as->as_vbase1 & PAGE_FRAME) == as->as_vbase1);
	KASSERT((as->as_ptable1[0] & PAGE_FRAME) == as->as_ptable1[0]);
	KASSERT((as->as_vbase2 & PAGE_FRAME) == as->as_vbase2);
	KASSERT((as->as_ptable2[0] & PAGE_FRAME) == as->as_ptable2[0]);
	KASSERT((as->as_pstacktable[0] & PAGE_FRAME) == as->as_pstacktable[0]);

	vbase1 = as->as_vbase1;
	vtop1 = vbase1 + as->as_npages1 * PAGE_SIZE;
	vbase2 = as->as_vbase2;
	vtop2 = vbase2 + as->as_npages2 * PAGE_SIZE;
	stackbase = USERSTACK - DUMBVM_STACKPAGES * PAGE_SIZE;
	stacktop = USERSTACK;

	if (faultaddress >= vbase1 && faultaddress < vtop1) {
		paddr = (faultaddress - vbase1) + as->as_ptable1[0];
	}
	else if (faultaddress >= vbase2 && faultaddress < vtop2) {
		paddr = (faultaddress - vbase2) + as->as_ptable2[0];
	}
	else if (faultaddress >= stackbase && faultaddress < stacktop) {
		paddr = (faultaddress - stackbase) + as->as_pstacktable[0];
	}
	else {
		return EFAULT;
	}

	/* make sure it's page-aligned */
	KASSERT((paddr & PAGE_FRAME) == paddr);

	/* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();

	//kprintf("NUM_TLB %d\n", NUM_TLB);
	//kprintf("DONE LOADING: %d\n", as->done_load_elf);
	for (i=0; i<NUM_TLB; i++) {
		tlb_read(&ehi, &elo, i);
		if (elo & TLBLO_VALID) {
			continue;
		}
		ehi = faultaddress;
		elo = paddr | TLBLO_DIRTY | TLBLO_VALID;

		// Add elo if load elf has completed
		//kprintf("DONE LOADING: %d", as->done_load_elf);
		if(as->done_load_elf && ehi >= vbase1 && ehi < vtop1){
			elo &= ~TLBLO_DIRTY;
		}

		DEBUG(DB_VM, "dumbvm: 0x%x -> 0x%x\n", faultaddress, paddr);
		tlb_write(ehi, elo, i);
		splx(spl);
		return 0;
	}

	//kprintf("dumbvm: Ran out of TLB entries - overwriting existing entry \n");
	ehi = faultaddress;
	elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
	
	if(as->done_load_elf && ehi >= vbase1 && ehi < vtop1){
		elo &= ~TLBLO_DIRTY;
	}

	DEBUG(DB_VM, "dumbvm: 0x%x -> 0x%x\n", faultaddress, paddr);
	tlb_random(ehi, elo);

	splx(spl);
	return 0;
	//return EFAULT;
}

struct addrspace *
as_create(void)
{
	struct addrspace *as = kmalloc(sizeof(struct addrspace));
	if (as==NULL) {
		return NULL;
	}

	as->done_load_elf = false;
	as->as_vbase1 = 0;
	as->as_ptable1 = NULL;
	as->as_npages1 = 0;
	as->as_vbase2 = 0;
	as->as_ptable2 = NULL;
	as->as_npages2 = 0;
	as->as_pstacktable = NULL;

	return as;
}

void
as_destroy(struct addrspace *as)
{
	for(size_t i = 0; i < as->as_npages1; i++){
		free_kpages(PADDR_TO_KVADDR(as->as_ptable1[i]));
	}

	for(size_t i = 0; i < as->as_npages2; i++){
		free_kpages(PADDR_TO_KVADDR(as->as_ptable2[i]));
	}

	for(size_t i = 0; i < DUMBVM_STACKPAGES; i++){
		free_kpages(PADDR_TO_KVADDR(as->as_pstacktable[i]));
	}

	kfree(as->as_ptable1);
	kfree(as->as_ptable2);
	kfree(as->as_pstacktable);
	kfree(as);
}

void
as_activate(void)
{
	int i, spl;
	struct addrspace *as;

	as = curproc_getas();
#ifdef UW
        /* Kernel threads don't have an address spaces to activate */
#endif
	if (as == NULL) {
		return;
	}

	/* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();

	for (i=0; i<NUM_TLB; i++) {
		tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	}

	splx(spl);
}

void
as_deactivate(void)
{
	/* nothing */
}

int
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t sz,
		 int readable, int writeable, int executable)
{
	size_t npages; 

	/* Align the region. First, the base... */
	sz += vaddr & ~(vaddr_t)PAGE_FRAME;
	vaddr &= PAGE_FRAME;

	/* ...and now the length. */
	sz = (sz + PAGE_SIZE - 1) & PAGE_FRAME;

	npages = sz / PAGE_SIZE;

	/* We don't use these - all pages are read-write */
	(void)readable;
	(void)writeable;
	(void)executable;

	if (as->as_vbase1 == 0) {
		as->as_vbase1 = vaddr;
		as->as_npages1 = npages;
		as->as_ptable1 = kmalloc(npages * sizeof(paddr_t));//sizeof(paddr_t*));
		//as->as_ptable1[0] = 0;
		//as->as_ptable1[0] = readable;
		return 0;
	}

	if (as->as_vbase2 == 0) {
		as->as_vbase2 = vaddr;
		as->as_npages2 = npages;
		as->as_ptable2 = kmalloc(npages * sizeof(paddr_t));//sizeof(paddr_t*));
		//as->as_ptable2[0] = 0;
		//as->as_ptable2[0] = writeable;
		return 0;
	}

	/*
	 * Support for more than two regions is not available.
	 */
	kprintf("dumbvm: Warning: too many regions\n");
	return EUNIMP;
}

/*static
void
as_zero_region(paddr_t paddr, unsigned npages)
{
	bzero((void *)PADDR_TO_KVADDR(paddr), npages * PAGE_SIZE);
}*/

static
void
alloc_ptable_frames(paddr_t* ptable, size_t num_pages){
	for(size_t i = 0; i < num_pages; i++){
		paddr_t ptablepage = getppages(1);
		ptable[i] = ptablepage;
	}
}

static
void
as_zero_regions(paddr_t* ptable, size_t num_pages){
	for(size_t i = 0; i < num_pages; i++){
		bzero((void *)PADDR_TO_KVADDR(ptable[i]), 1 * PAGE_SIZE);
	}
}

int
as_prepare_load(struct addrspace *as)
{
	KASSERT(as->as_ptable1 != NULL);
	KASSERT(as->as_ptable2 != NULL);

	as->as_pstacktable = kmalloc(DUMBVM_STACKPAGES * sizeof(paddr_t));
	KASSERT(as->as_pstacktable != NULL);

	alloc_ptable_frames(as->as_ptable1, as->as_npages1);
	if (as->as_ptable1[0] == 0) {
		return ENOMEM;
	}

	alloc_ptable_frames(as->as_ptable2, as->as_npages2);
	if (as->as_ptable2[0] == 0) {
		return ENOMEM;
	}

	alloc_ptable_frames(as->as_pstacktable, DUMBVM_STACKPAGES);
	if (as->as_pstacktable[0] == 0) {
		return ENOMEM;
	}
	
	as_zero_regions(as->as_ptable1, as->as_npages1);
	as_zero_regions(as->as_ptable2, as->as_npages2);
	as_zero_regions(as->as_pstacktable, DUMBVM_STACKPAGES);
	//as_zero_region(as->as_ptable1, as->as_npages1);
	//as_zero_region(as->as_ptable2, as->as_npages2);
	//as_zero_region(as->as_pstacktable, DUMBVM_STACKPAGES);

	return 0;
}

int
as_complete_load(struct addrspace *as)
{
	(void)as;
	return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr, char **argv, size_t argc)
{
	KASSERT(as->as_pstacktable != NULL);

	userptr_t *stack_arr = kmalloc((argc + 1) * sizeof(userptr_t));

	*stackptr = USERSTACK;
	int err;

	// Put args onto the stack
	// (Malloc space as you go along)
	for(size_t i = 0; i < argc + 1; i++){

		// Break early if NULL and 
		// put args on the top of the stack and increment stack pointer (do you have to do this?)
		if(i == argc){

			stack_arr[i] = (userptr_t) NULL;

			*stackptr = ROUNDUP(*stackptr - ((argc + 1) * sizeof(userptr_t)) - 8, 8);

			err = copyout(stack_arr, (userptr_t) *stackptr, (argc + 1) * sizeof(userptr_t));
			if(err){
				return err;
			}

			break;
		}

		size_t curr_len = strlen(argv[i]) + 1;

		// Modify stackptr as you go along (including the first one)
		*stackptr = ROUNDUP(*stackptr - curr_len - 8, 8);
		stack_arr[i] = (userptr_t) *stackptr;

		err = copyoutstr(argv[i], (userptr_t) *stackptr, curr_len, NULL);//got);
		if(err){
			return err;
		}
	}

	return err;
}

static
void
ptable_copy(paddr_t *new_pt, paddr_t *old_pt, size_t npages){
	for (size_t i = 0; i < npages; i++){
		memmove((void *)PADDR_TO_KVADDR(new_pt[i]),
			(const void *)PADDR_TO_KVADDR(old_pt[i]),
			PAGE_SIZE);
	}
}

int
as_copy(struct addrspace *old, struct addrspace **ret)
{
	struct addrspace *new;

	new = as_create();
	if (new==NULL) {
		return ENOMEM;
	}

	new->as_vbase1 = old->as_vbase1;
	new->as_npages1 = old->as_npages1;
	new->as_vbase2 = old->as_vbase2;
	new->as_npages2 = old->as_npages2;

	new->as_ptable1 = kmalloc(new->as_npages1 * sizeof(paddr_t));
	new->as_ptable2 = kmalloc(new->as_npages2 * sizeof(paddr_t));

	/* (Mis)use as_prepare_load to allocate some physical memory. */
	if (as_prepare_load(new)) {
		as_destroy(new);
		return ENOMEM;
	}

	KASSERT(new->as_ptable1 != NULL);
	KASSERT(new->as_ptable2 != NULL);
	KASSERT(new->as_pstacktable != NULL);

	ptable_copy(new->as_ptable1, old->as_ptable1, new->as_npages1);
	ptable_copy(new->as_ptable2, old->as_ptable2, new->as_npages2);
	ptable_copy(new->as_pstacktable, old->as_pstacktable, DUMBVM_STACKPAGES);

	*ret = new;
	return 0;
}
