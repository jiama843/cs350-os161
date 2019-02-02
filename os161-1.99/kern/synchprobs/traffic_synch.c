#include <types.h>
#include <lib.h>
#include <synchprobs.h>
#include <synch.h>
#include <opt-A1.h>

/* 
 * This simple default synchronization mechanism allows only vehicle at a time
 * into the intersection.   The intersectionSem is used as a a lock.
 * We use a semaphore rather than a lock so that this code will work even
 * before locks are implemented.
 */

/* 
 * Replace this default synchronization mechanism with your own (better) mechanism
 * needed for your solution.   Your mechanism may use any of the available synchronzation
 * primitives, e.g., semaphores, locks, condition variables.   You are also free to 
 * declare other global variables if your solution requires them.
 */

/*
 * replace this with declarations of any synchronization and other variables you need here
 */

static struct lock *dir_lock = 0; // Allows for change of direction restriction

// Keep unique condition variables for each destination
static struct cv *cv_DN = 0;    
static struct cv *cv_DS = 0;
static struct cv *cv_DE = 0;
static struct cv *cv_DW = 0;

// Keep track of all directions that a vechicle can collide with 
static int rightON = 0;
static int rightOS = 0;
static int rightOE = 0;
static int rightOW = 0;

static int straightON = 0;
static int straightOS = 0;
static int straightOE = 0;
static int straightOW = 0;

static int leftON = 0; 
static int leftOS = 0;
static int leftOE = 0;
static int leftOW = 0;

// return the number associated with direction
static int get_direction(Direction origin, Direction destination){
  if (origin == north && destination == west){
    return rightON;
  }
  else if (origin == south && destination == east){
    return rightOS;
  }
  else if (origin == east && destination == north){
    return rightOE;
  }
  else if (origin == west && destination == south){
    return rightOW;
  }
  else if (origin == north && destination == south){
    return straightON;
  }
  else if (origin == south && destination == north){
    return straightOS;
  }
  else if (origin == east && destination == west){
    return straightOE;
  }
  else if (origin == west && destination == east){
    return straightOW;
  }
  else if (origin == north && destination == east){
    return leftON;
  }
  else if (origin == south && destination == west){
    return leftOS;
  }
  else if (origin == east && destination == south){
    return leftOE;
  }
  else {
    return leftOW;
  }
}

// return the number associated with direction
static void restrict_direction(Direction origin, Direction destination, int change){
  if (origin == north && destination == west){
    leftOS += change;
    straightOE += change;
  }
  else if (origin == south && destination == east){
    leftON += change; 
    straightOW += change;
  }
  else if (origin == east && destination == north){
    leftOW += change;
    straightOS += change;
  }
  else if (origin == west && destination == south){
    leftOE += change;
    straightON += change;
  }
  else if (origin == north && destination == south){
    leftOS += change;
    leftOE += change;
    leftOW += change;
    straightOE += change;
    straightOW += change;
    rightOW += change;
  }
  else if (origin == south && destination == north){
    leftON += change;
    leftOE += change;
    leftOW += change;
    straightOE += change;
    straightOW += change;
    rightOE += change;
  }
  else if (origin == east && destination == west){
    leftON += change;
    leftOS += change;
    leftOW += change;
    straightON += change;
    straightOS += change;
    rightON += change;
  }
  else if (origin == west && destination == east){
    leftON += change;
    leftOS += change;
    leftOE += change;
    straightON += change;
    straightOS += change;
    rightOS += change;
  }
  else if (origin == north && destination == east){
    leftOS += change;
    leftOE += change;
    leftOW += change;
    straightOS += change;
    straightOE += change;
    straightOW += change;
    rightOS += change;
  }
  else if (origin == south && destination == west){
    leftON += change;
    leftOE += change;
    leftOW += change;
    straightON += change;
    straightOE += change;
    straightOW += change;
    rightON += change;
  }
  else if (origin == east && destination == south){
    leftON += change;
    leftOS += change;
    leftOW += change;
    straightON += change;
    straightOS += change;
    straightOW += change;
    rightOW += change;
  }
  else {
    leftON += change;
    leftOS += change;
    leftOE += change;
    straightON += change;
    straightOS += change;
    straightOE += change;
    rightOE += change;
  }
}

// Set condition variables
static void set_cv(Direction destination, struct lock *lk){
  if (destination == north){
    cv_wait(cv_DN, lk);
  }
  else if (destination == south){
    cv_wait(cv_DS, lk);
  }
  else if (destination == east) {
    cv_wait(cv_DE, lk);
  }
  else if (destination == west){
    cv_wait(cv_DW, lk);
  }
}


// Wake conditions
static void wake_cvs(struct lock *lk){
  if (rightOE == 0 || straightOS == 0 || leftOW == 0){
    cv_broadcast(cv_DN, lk);
  }
  if (rightOW == 0 || straightON == 0 || leftOE == 0){
    cv_broadcast(cv_DS, lk);
  }
  if (rightOS == 0 || straightOW == 0 || leftON == 0){
    cv_broadcast(cv_DE, lk);
  }
  if (rightON == 0 || straightOE == 0 || leftOS == 0){
    cv_broadcast(cv_DW, lk);
  }
}

/* 
 * The simulation driver will call this function once before starting
 * the simulation
 *
 * You can use it to initialize synchronization and other variables.
 * 
 */
void
intersection_sync_init(void)
{
  cv_DN = cv_create("DN");
  if (cv_DN == NULL) {
    panic("could not create cv DN");
  }

  cv_DS = cv_create("DS");
  if (cv_DS == NULL) {
    panic("could not create cv DS");
  }

  cv_DE = cv_create("DE");
  if (cv_DE == NULL) {
    panic("could not create cv DE");
  }

  cv_DW = cv_create("DW");
  if (cv_DW == NULL) {
    panic("could not create cv DW");
  }

  dir_lock = lock_create("dir_lock");
  if (dir_lock == NULL) {
    panic("could not create dir_lock");
  }
 
  return;
}


/* 
 * The simulation driver will call this function once after
 * the simulation has finished
 *
 * You can use it to clean up any synchronization and other variables.
 *
 */
void
intersection_sync_cleanup(void)
{
  KASSERT(cv_DN != NULL);
  cv_destroy(cv_DN);

  KASSERT(cv_DS != NULL);
  cv_destroy(cv_DS);

  KASSERT(cv_DE != NULL);
  cv_destroy(cv_DE);

  KASSERT(cv_DW != NULL);
  cv_destroy(cv_DW);

  KASSERT(dir_lock != NULL);
  lock_destroy(dir_lock);
}


/*
 * The simulation driver will call this function each time a vehicle
 * tries to enter the intersection, before it enters.
 * This function should cause the calling simulation thread 
 * to block until it is OK for the vehicle to enter the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle is arriving
 *    * destination: the Direction in which the vehicle is trying to go
 *
 * return value: none
 */

void
intersection_before_entry(Direction origin, Direction destination) 
{

  //lock_acquire(get_lock(origin, destination));
  lock_acquire(dir_lock);
  while (get_direction(origin, destination) > 0){
    set_cv(destination, dir_lock);
  }
  
  restrict_direction(origin, destination, 1); 
  lock_release(dir_lock);
}


/*
 * The simulation driver will call this function each time a vehicle
 * leaves the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle arrived
 *    * destination: the Direction in which the vehicle is going
 *
 * return value: none
 */

void
intersection_after_exit(Direction origin, Direction destination) 
{

  lock_acquire(dir_lock);

  restrict_direction(origin, destination, -1);
  wake_cvs(dir_lock);

  lock_release(dir_lock);

}
