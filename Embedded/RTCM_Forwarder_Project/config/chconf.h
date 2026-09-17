#ifndef _CHCONF_H_
#define _CHCONF_H_

#define CH_CFG_ST_RESOLUTION             32
#define CH_CFG_ST_FREQUENCY              1000
#define CH_CFG_TIME_TYPES_SIZE           32

#define CH_DBG_STATISTICS                FALSE
#define CH_DBG_SYSTEM_STATE_CHECK        TRUE
#define CH_DBG_ENABLE_CHECKS             TRUE
#define CH_DBG_ENABLE_ASSERTS            TRUE

#define CH_CFG_USE_REGISTRY              TRUE
#define CH_CFG_USE_SEMAPHORES            TRUE
#define CH_CFG_USE_MUTEXES               TRUE
#define CH_CFG_USE_CONDVARS              FALSE
#define CH_CFG_USE_EVENTS                TRUE
#define CH_CFG_USE_MESSAGES              FALSE
#define CH_CFG_USE_MAILBOXES             FALSE
#define CH_CFG_USE_HEAP                  FALSE
#define CH_CFG_USE_MEMCORE               TRUE
#define CH_CFG_USE_MEMPOOLS              FALSE
#define CH_CFG_USE_OBJ_FIFOS             FALSE
#define CH_CFG_USE_DYNAMIC               FALSE


// Enable or disable kernel trace support
#define CH_DBG_ENABLE_TRACE           FALSE

// Enable time measurement API (Timers, etc.)
#define CH_CFG_USE_TM                TRUE

// If set to TRUE, the system will not create the idle thread.
// Usually FALSE unless you want to disable the idle thread.
#define CH_CFG_NO_IDLE_THREAD        FALSE

// Optimize for speed (TRUE) or size (FALSE)
#define CH_CFG_OPTIMIZE_SPEED        FALSE

// Time slice duration in system ticks for round-robin threads.
// 0 means time slicing is disabled.
#define CH_CFG_TIME_QUANTUM          10

// System time delta configuration for system timers.
// Usually 0 unless you have specific timing needs.
#define CH_CFG_ST_TIMEDELTA          0

/*
 * Add other kernel configuration macros below as needed for your project.
 * You can find the full list in chconf.template.h inside the ChibiOS source tree.
 */


#endif /* _CHCONF_H_ */

