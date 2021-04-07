/****************************************************************************
*
* Copyright (c) 2014 - 2016 Samsung Electronics Co., Ltd. All rights reserved
*
****************************************************************************/

#ifndef __FUNCTOR_H
#define __FUNCTOR_H

/**
 * Minimal Functor (no returns, no args other than self).
 */
struct functor {
	/**
	 * The callback invoked by functor_call().
	 *
	 * A pointer to the functor itself is passed to the call.
	 *
	 * Typically the implementation wil upcast this (container_of)
	 * to access a container context.
	 */
	void (*call)(struct functor *f);
};

/**
 * Initialise this functor.
 */
static inline void functor_init(struct functor *f, void (*call)(struct functor *f))
{
	f->call = call;
}

/**
 * Invoke this functor.
 */
static inline void functor_call(struct functor *f)
{
	f->call(f);
}

#endif  /* __FUNCTOR_H */

