// MIT License
// Copyright (c) 2023 Lauri Lorenzo Fiestas
// https://github.com/PrinssiFiestas/LockFreeC/blob/main/LICENSE

#ifndef LFC_H_INCLUDED
#define LFC_H_INCLUDED 1

#if !__cplusplus
#include <stdatomic.h>
#else
#include <atomic>
#endif
#include <stdalign.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>


// ----------------------------------------------------------------------------
//
//          API REFERENCE
//
// ----------------------------------------------------------------------------


// ------------------------------------------------------------
// Wait-free Single Producer Single Consumer type generic queue

/** Generic SPSC queue type.*/
#define LFQueue(T) T*

/** Create generic queue. C only.
 * @p buffer_length specifies how many elements of type @p T fit in @p buffer.
 * For best performance, buffer size shuold not exceed 32 KB to fit in L3 cache.
 * If @p buffer is not provided, it will be created on stack if local scope, or
 * statically if global scope.
 */
#define lf_queue(T,/* void* buffer = static_buffer, size_t buffer_length */...) (T*)&LF_OVERLOAD2(__VA_ARGS__, LF_QUEUE_WITH_BUFFER, LF_QUEUE_WOUT_BUFFER,)(T,__VA_ARGS__)

/** Generic enqueue.
 * @return `true` if @p elem got successfully enqueued, `false` if queue was
 * full, in which case the element was not queued.
 */
#define lf_enqueue(/* LFQueue(T) */queue,/* T */elem) lf_spsc_enqueue((LFSPSCQueue*)(queue), &(struct { LF_TYPEOF(*(queue))_; }) { elem }, sizeof(elem))

/** Generic dequeue.
 * @return @p out if @p queue was not empty, `NULL` otherwise. If @p out is not
 * provided, the returned element will be allocated on stack.
 */
#define lf_dequeue(/* LFQueue(T) queue, T* out = static_buffer */...) LF_OVERLOAD2(__VA_ARGS__, LF_DEQUEUE_WITH_BUFFER, LF_DEQUEUE_WOUT_BUFFER,)(__VA_ARGS__)

// ------------------------------------------------------------
// Wait-free Single Producer Single Consumer queue

static_assert(ATOMIC_SHORT_LOCK_FREE == 2, "No suitable lock-free integers available");
#if ATOMIC_LLONG_LOCK_FREE == 2
/** Guaranteed lock-free when used as atomic.*/
typedef unsigned long long LFUint;
#elif ATOMIC_INT_LOCK_FREE == 2
/** Guaranteed lock-free when used as atomic.*/
typedef unsigned LFUint;
#else
/** Guaranteed lock-free when used as atomic.*/
typedef unsigned short LFUint;
#endif

#if !__cplusplus
#define LFAtomic(...) _Atomic(__VA_ARGS__)
#define LF_RESTRICT restrict
#define LF_USING_NAMESPACE_STD
#else
#define LFAtomic(...) std::atomic<__VA_ARGS__>
#define LF_RESTRICT __restrict
#define LF_USING_NAMESPACE_STD using namespace std
#endif

#if __GNUC__
#define LF_NONNULL_ARGS(...) __attribute__((nonnull (__VA_ARGS__)))
#else
#define LF_NONNULL_ARGS(...)
#endif

// TODO document these too.

typedef struct lf_spsc_queue
{
    alignas(64) LFAtomic(LFUint) head;
    alignas(64) LFAtomic(LFUint) tail;
    void*  buffer;
    size_t buffer_length;
} LFSPSCQueue;

static inline LFUint lf_index(LFUint x, LFUint queue_buffer_size);

LF_NONNULL_ARGS()
static inline bool lf_spsc_enqueue(LFSPSCQueue* queue, const void*LF_RESTRICT data, size_t data_size)
{
    LF_USING_NAMESPACE_STD;
    LFUint old_head = atomic_load_explicit(&queue->head, memory_order_acquire);
    if (lf_index(old_head, queue->buffer_length) ==
        lf_index(atomic_load_explicit(&queue->tail, memory_order_relaxed) - 1, queue->buffer_length))
        return false;

    memcpy((char*)queue->buffer + data_size * lf_index(old_head, queue->buffer_length), data, data_size);
    LFUint new_head = old_head + 1;
    atomic_store_explicit(&queue->head, new_head, memory_order_release);

    return true;
}

LF_NONNULL_ARGS()
static inline void* lf_spsc_dequeue(LFSPSCQueue* queue, void*LF_RESTRICT out, size_t out_size)
{
    LF_USING_NAMESPACE_STD;
    LFUint old_tail = atomic_load_explicit(&queue->tail, memory_order_acquire);
    if (old_tail == atomic_load_explicit(&queue->head, memory_order_relaxed))
        return NULL;

    LFUint new_tail = old_tail + 1;
    atomic_store_explicit(&queue->tail, new_tail, memory_order_release);

    memcpy(out, (char*)queue->buffer + out_size * lf_index(old_tail, queue->buffer_length), out_size);
    return out;
}


// ----------------------------------------------------------------------------
//
//          END OF API REFERENCE
//
//          Code below is for internal usage and may change without notice.
//
// ----------------------------------------------------------------------------


static inline LFUint lf_index(LFUint x, LFUint queue_buffer_size)
{
    const bool queue_buffer_size_is_power_of_2 = (queue_buffer_size & (queue_buffer_size - 1)) == 0;
    assert(queue_buffer_size_is_power_of_2); (void)queue_buffer_size_is_power_of_2;
    assert(queue_buffer_size > 0);
    return x & (queue_buffer_size - 1);
}

#if __STDC_VERSION__ >= 202311L
#define LF_TYPEOF(...) typeof(__VA_ARGS__)
#elif __cplusplus
#define LF_TYPEOF(...) decltype(__VA_ARGS__)
#else
#define LF_TYPEOF(...) __typeof__(__VA_ARGS__)
#endif

#define LF_OVERLOAD2(_0, _1, RESOLVED, ...) RESOLVED

#define LF_QUEUE_WOUT_BUFFER(T, BUFFER_LENGTH) \
    (LFSPSCQueue){.buffer = (T[BUFFER_LENGTH]){0}, .buffer_length = (BUFFER_LENGTH) }
#define LF_QUEUE_WITH_BUFFER(T, BUFFER, BUFFER_LENGTH) \
    (LFSPSCQueue){.buffer = (BUFFER), .buffer_length = (BUFFER_LENGTH) }

#define LF_DEQUEUE_WOUT_BUFFER(QUEUE) \
    (LF_TYPEOF(QUEUE))lf_spsc_dequeue((LFSPSCQueue*)(QUEUE), &(LF_TYPEOF(*(QUEUE))){0}, sizeof(*(QUEUE)))
#define LF_DEQUEUE_WITH_BUFFER(QUEUE, BUFFER) \
    (LF_TYPEOF(QUEUE))lf_spsc_dequeue((LFSPSCQueue*)(QUEUE), (BUFFER), sizeof(*(QUEUE) = (BUFFER)))

#endif // LFC_H_INCLUDED
