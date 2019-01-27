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
//static struct semaphore *intersectionSem;

// Keep locks for each direction cv
static struct lock *lk_rightON = 0;
static struct lock *lk_rightOS = 0;
static struct lock *lk_rightOE = 0;
static struct lock *lk_rightOW = 0;

static struct lock *lk_straightON = 0;
static struct lock *lk_straightOS = 0;
static struct lock *lk_straightOE = 0;
static struct lock *lk_straightOW = 0;

static struct lock *lk_leftON = 0; 
static struct lock *lk_leftOS = 0;
static struct lock *lk_leftOE = 0;
static struct lock *lk_leftOW = 0;

static struct lock *dir_lock = 0; // Allows for change of direction restriction

// Keep unique condition variables for each direction
static struct cv *cv_rightON = 0;
static struct cv *cv_rightOS = 0;
static struct cv *cv_rightOE = 0;
static struct cv *cv_rightOW = 0;

static struct cv *cv_straightON = 0; 
static struct cv *cv_straightOS = 0;
static struct cv *cv_straightOE = 0;
static struct cv *cv_straightOW = 0;

static struct cv *cv_leftON = 0;    
static struct cv *cv_leftOS = 0;
static struct cv *cv_leftOE = 0;
static struct cv *cv_leftOW = 0;

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
  }
  else if (origin == south && destination == east){
    leftON += change; 
  }
  else if (origin == east && destination == north){
    leftOW += change;
  }
  else if (origin == west && destination == south){
    leftOS += change;
  }
  else if (origin == north && destination == south){
    leftOS += change;
    leftOE += change;
    leftOW += change;
    straightOE += change;
    straightOW += change;
  }
  else if (origin == south && destination == north){
    leftON += change;
    leftOE += change;
    leftOW += change;
    straightOE += change;
    straightOW += change;
  }
  else if (origin == east && destination == west){
    leftON += change;
    leftOS += change;
    leftOW += change;
    straightON += change;
    straightOS += change;
  }
  else if (origin == west && destination == east){
    leftON += change;
    leftOS += change;
    leftOE += change;
    straightON += change;
    straightOS += change;
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

// return the respective lock
static struct lock *get_lock(Direction origin, Direction destination){
  if (origin == north && destination == west){
    return lk_rightON;
  }
  else if (origin == south && destination == east){
    return lk_rightOS;
  }
  else if (origin == east && destination == north){
    return lk_rightOE;
  }
  else if (origin == west && destination == south){
    return lk_rightOW;
  }
  else if (origin == north && destination == south){
    return lk_straightON;
  }
  else if (origin == south && destination == north){
    return lk_straightOS;
  }
  else if (origin == east && destination == west){
    return lk_straightOE;
  }
  else if (origin == west && destination == east){
    return lk_straightOW;
  }
  else if (origin == north && destination == east){
    return lk_leftON;
  }
  else if (origin == south && destination == west){
    return lk_leftOS;
  }
  else if (origin == east && destination == south){
    return lk_leftOE;
  }
  else {
    return lk_leftOW;
  }
}

// Set condition variables
static void set_cv(Direction origin, Direction destination, struct lock *lk){
  if (origin == north && destination == west){
    cv_wait(cv_rightON, lk);
  }
  else if (origin == south && destination == east){
    cv_wait(cv_rightOS, lk);
  }
  else if (origin == east && destination == north){
    cv_wait(cv_rightOE, lk);
  }
  else if (origin == west && destination == south){
    cv_wait(cv_rightOW, lk);
  }
  else if (origin == north && destination == south){
    cv_wait(cv_straightON, lk);
  }
  else if (origin == south && destination == north){
    cv_wait(cv_straightOS, lk);
  }
  else if (origin == east && destination == west){
    cv_wait(cv_straightOE, lk);
  }
  else if (origin == west && destination == east){
    cv_wait(cv_straightOW, lk);
  }
  else if (origin == north && destination == east){
    cv_wait(cv_leftON, lk);
  }
  else if (origin == south && destination == west){
    cv_wait(cv_leftOS, lk);
  }
  else if (origin == east && destination == south){
    cv_wait(cv_leftOE, lk);
  }
  else {
    cv_wait(cv_leftOW, lk);
  }
}

// Wake conditions
static void wake_cvs(){
  if (rightON == 0){
    cv_broadcast(cv_rightON, lk_rightON);
  }
  if (rightOS == 0){
    cv_broadcast(cv_rightOS, lk_rightOS);
  }
  if (rightOE == 0){
    cv_broadcast(cv_rightOE, lk_rightOE);
  }
  if (rightOW == 0){
    cv_broadcast(cv_rightOW, lk_rightOW);
  }
  if (straightON == 0){
    cv_broadcast(cv_straightON, lk_straightON);
  }
  if (straightOS == 0){
    cv_broadcast(cv_straightOS, lk_straightOS);
  }
  if (straightOE == 0){
    cv_broadcast(cv_straightOE, lk_straightOE);
  }
  if (straightOW == 0){
    cv_broadcast(cv_straightOW, lk_straightOW);
  }
  if (leftON == 0){
    cv_broadcast(cv_leftON, lk_leftON);
  }
  if (leftOS == 0){
    cv_broadcast(cv_leftOS, lk_leftOS);
  }
  if (leftOE == 0){
    cv_broadcast(cv_leftOE, lk_leftOE);
  }
  if (leftOW == 0){
    cv_broadcast(cv_leftOW, lk_leftOW);
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
  /* replace this default implementation with your own implementation */

  /*intersectionSem = sem_create("intersectionSem",1);
  if (intersectionSem == NULL) {
    panic("could not create intersection semaphore");
  }*/

  cv_rightON = cv_create("rightON");
  if (cv_rightON == NULL) {
    panic("could not create cv rightON");
  } 

  cv_rightOS = cv_create("rightOS");
  if (cv_rightOS == NULL) {
    panic("could not create cv rightOS");
  }

  cv_rightOE = cv_create("rightOE");
  if (cv_rightOE == NULL) {
    panic("could not create cv rightOE");
  }

  cv_rightOW = cv_create("rightOW");
  if (cv_rightOW == NULL) {
    panic("could not create cv rightOW");
  }

  cv_straightON = cv_create("straightON");
  if (cv_straightON == NULL) {
    panic("could not create cv straightON");
  }

  cv_straightOS = cv_create("straightOS");
  if (cv_straightOS == NULL) {
    panic("could not create cv straightOS");
  }

  cv_straightOE = cv_create("straightOE");
  if (cv_straightOE == NULL) {
    panic("could not create cv straightOE");
  }

  cv_straightOW = cv_create("straightOW");
  if (cv_straightOW == NULL) {
    panic("could not create cv straightOW");
  }
 
  cv_leftON = cv_create("leftON");
  if (cv_leftON == NULL) {
    panic("could not create cv leftON");
  }

  cv_leftOS = cv_create("leftOS");
  if (cv_leftOS == NULL) {
    panic("could not create cv leftOS");
  }

  cv_leftOE = cv_create("leftOE");
  if (cv_leftOE == NULL) {
    panic("could not create cv leftOE");
  }

  cv_leftOW = cv_create("leftOW");
  if (cv_leftOW == NULL) {
    panic("could not create cv leftOW");
  }

  lk_rightON = lock_create("rightON");
  if (lk_rightON == NULL) {
    panic("could not create lk rightON");
  }

  lk_rightOS = lock_create("rightOS");
  if (lk_rightOS == NULL) {
    panic("could not create lk rightOS");
  } 

  lk_rightOE = lock_create("rightOE");
  if (lk_rightOE == NULL) {
    panic("could not create lk rightOE");
  }

  lk_rightOW = lock_create("rightOW");
  if (lk_rightOW == NULL) {
    panic("could not create lk rightOW");
  }

  lk_straightON = lock_create("straightON");
  if (lk_straightON == NULL) {
    panic("could not create lk straightON");
  }

  lk_straightOS = lock_create("straightOS");
  if (lk_straightOS == NULL) {
    panic("could not create lk straightOS");
  }

  lk_straightOE = lock_create("straightOE");
  if (lk_straightOE == NULL) {
    panic("could not create lk straightOE");
  }

  lk_straightOW = lock_create("straightOW");
  if (lk_straightOW == NULL) {
    panic("could not create lk straightOW");
  }

  lk_leftON = lock_create("leftON");
  if (lk_leftON == NULL) {
    panic("could not create lk leftON");
  }

  lk_leftOS = lock_create("leftOS");
  if (lk_leftOS == NULL) {
    panic("could not create lk leftOS");
  }

  lk_leftOE = lock_create("leftOE");
  if (lk_leftOE == NULL) {
    panic("could not create lk leftOE");
  }

  lk_leftOW = lock_create("leftOW");
  if (lk_leftOW == NULL) {
    panic("could not create lk leftOW");
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
  /* replace this default implementation with your own implementation */
  /*KASSERT(intersectionSem != NULL);
  sem_destroy(intersectionSem);*/

  KASSERT(cv_rightON != NULL);
  cv_destroy(cv_rightON);

  KASSERT(cv_rightOS != NULL);
  cv_destroy(cv_rightOS);

  KASSERT(cv_rightOE != NULL);
  cv_destroy(cv_rightOE);

  KASSERT(cv_rightOW != NULL);
  cv_destroy(cv_rightOW);

  KASSERT(cv_straightON != NULL);
  cv_destroy(cv_straightON);

  KASSERT(cv_straightOS != NULL);
  cv_destroy(cv_straightOS);

  KASSERT(cv_straightOE != NULL);
  cv_destroy(cv_straightOE);

  KASSERT(cv_straightOW != NULL);
  cv_destroy(cv_straightOW);

  KASSERT(cv_leftON != NULL);
  cv_destroy(cv_leftON);

  KASSERT(cv_leftOS != NULL);
  cv_destroy(cv_leftOS);

  KASSERT(cv_leftOE != NULL);
  cv_destroy(cv_leftOE);

  KASSERT(cv_leftOW != NULL);
  cv_destroy(cv_leftOW);

  KASSERT(lk_rightON != NULL);
  lock_destroy(lk_rightON);

  KASSERT(lk_rightOS != NULL);
  lock_destroy(lk_rightOS);

  KASSERT(lk_rightOE != NULL);
  lock_destroy(lk_rightOE);

  KASSERT(lk_rightOW != NULL);
  lock_destroy(lk_rightOW);

  KASSERT(lk_straightON != NULL);
  lock_destroy(lk_straightON);

  KASSERT(lk_straightOS != NULL);
  lock_destroy(lk_straightOS);

  KASSERT(lk_straightOE != NULL);
  lock_destroy(lk_straightOE);

  KASSERT(lk_straightOW != NULL);
  lock_destroy(lk_straightOW);

  KASSERT(lk_leftON != NULL);
  lock_destroy(lk_leftON);

  KASSERT(lk_leftOS != NULL);
  lock_destroy(lk_leftOS);

  KASSERT(lk_leftOE != NULL);
  lock_destroy(lk_leftOE);

  KASSERT(lk_leftOW != NULL);
  lock_destroy(lk_leftOW);

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
  /* replace this default implementation with your own implementation */
  //(void)origin;  /* avoid compiler complaint about unused parameter */
  //(void)destination; /* avoid compiler complaint about unused parameter */
  //KASSERT(intersectionSem != NULL);
  //P(intersectionSem);

  lock_acquire(get_lock(origin, destination));
  while (get_direction(origin, destination) > 0){
    set_cv(origin, destination, get_lock(origin, destination)); // POSSIBLE DEADLOCK/CONTEXT SWITCH
  }
  
  lock_acquire(dir_lock);
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
  /* replace this default implementation with your own implementation */
  //(void)origin;  /* avoid compiler complaint about unused parameter */
  //(void)destination; /* avoid compiler complaint about unused parameter */
  //KASSERT(intersectionSem != NULL);
  //V(intersectionSem);

  lock_acquire(dir_lock);

  restrict_direction(origin, destination, -1);
  wake_cvs();

  // Unsure about these two (may cause deadlock) 
  lock_release(dir_lock);
  lock_release(get_lock(origin, destination));

}
