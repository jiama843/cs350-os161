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
static struct semaphore *intersectionSem;
static struct cv *cv_left;
static struct cv *cv_straight;

static struct lock *lk_left;
static struct lock *lk_straight;
static struct lock *lk_origin;

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

  intersectionSem = sem_create("intersectionSem",1);
  if (intersectionSem == NULL) {
    panic("could not create intersection semaphore");
  }

  cv_left = cv_create("left");
  if (cv_left == NULL) {
    panic("could not create cv left");
  } 

  cv_straight = cv_create("straight");
  if (cv_straight == NULL) {
    panic("could not create cv straight");
  }

  lk_left = lock_create("left");
  if (lk_left == NULL) {
    panic("could not create lk left");
  }
 
  lk_straight = lock_create("straight");
  if (lk_straight == NULL) {
    panic("could not create lk straight");
  }

  lk_origin = lock_create("origin");
  if (lk_origin == NULL) {
    panic("could not create lk origin");
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
  KASSERT(intersectionSem != NULL);
  sem_destroy(intersectionSem);

  KASSERT(cv_left != NULL);
  cv_destroy(cv_left);

  KASSERT(cv_straight != NULL);
  cv_destroy(cv_straight);

  KASSERT(lk_left != NULL);
  lock_destroy(lk_left);

  KASSERT(lk_straight != NULL);
  lock_destroy(lk_straight);

  KASSERT(lk_origin != NULL);
  lock_destroy(lk_origin);
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
  (void)origin;  /* avoid compiler complaint about unused parameter */
  (void)destination; /* avoid compiler complaint about unused parameter */
  KASSERT(intersectionSem != NULL);
  P(intersectionSem);
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
  (void)origin;  /* avoid compiler complaint about unused parameter */
  (void)destination; /* avoid compiler complaint about unused parameter */
  KASSERT(intersectionSem != NULL);
  V(intersectionSem);
}
