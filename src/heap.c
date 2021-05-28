#include "heap.h"

#include <stdlib.h>

#include "../include/raft.h"

static void *defaultMalloc(void *data, size_t size)
{
    (void)data;
    return malloc(size);
}

static void defaultFree(void *data, void *ptr)
{
    (void)data;
    free(ptr);
}

static void *defaultCalloc(void *data, size_t nmemb, size_t size)
{
    (void)data;
    return calloc(nmemb, size);
}

static void *defaultRealloc(void *data, void *ptr, size_t size)
{
    (void)data;
    return realloc(ptr, size);
}

static void *defaultAlignedAlloc(void *data, size_t alignment, size_t size)
{
    (void)data;
#ifdef _WIN32
    return _aligned_malloc(size, alignment);
#elif defined(__APPLE__)
    void * p1; // original block
    void ** p2; // aligned block
    size_t offset = alignment + sizeof(void *) - 1;
    if ((p1 = (void *)malloc(size + offset)) == NULL)
        return NULL;
    p2 = (void **)(((uintptr_t)(p1) + offset) & ~(alignment - 1));
    p2[-1] = p1;
    return p2;
#else
    return aligned_alloc(alignment, size);
#endif
}

static void defaultAlignedFree(void *data, size_t alignment, void *ptr)
{
    (void)alignment;
#ifdef _WIN32
    (void)ptr;
    _aligned_free(data);
#elif defined(__APPLE__)
    (void)data;
    free(((void * *)ptr)[-1]);
#else
    defaultFree(data, ptr);
#endif
}

static struct raft_heap defaultHeap = {
    NULL,                /* data */
    defaultMalloc,       /* malloc */
    defaultFree,         /* free */
    defaultCalloc,       /* calloc */
    defaultRealloc,      /* realloc */
    defaultAlignedAlloc, /* aligned_alloc */
    defaultAlignedFree   /* aligned_free */
};

static struct raft_heap *currentHeap = &defaultHeap;

void *HeapMalloc(size_t size)
{
    return currentHeap->malloc(currentHeap->data, size);
}

void HeapFree(void *ptr)
{
    if (ptr == NULL) {
        return;
    }
    currentHeap->free(currentHeap->data, ptr);
}

void *HeapCalloc(size_t nmemb, size_t size)
{
    return currentHeap->calloc(currentHeap->data, nmemb, size);
}

void *HeapRealloc(void *ptr, size_t size)
{
    return currentHeap->realloc(currentHeap->data, ptr, size);
}

void *raft_malloc(size_t size)
{
    return HeapMalloc(size);
}

void raft_free(void *ptr)
{
    HeapFree(ptr);
}

void *raft_calloc(size_t nmemb, size_t size)
{
    return HeapCalloc(nmemb, size);
}

void *raft_realloc(void *ptr, size_t size)
{
    return HeapRealloc(ptr, size);
}

void *raft_aligned_alloc(size_t alignment, size_t size)
{
    return currentHeap->aligned_alloc(currentHeap->data, alignment, size);
}

void raft_aligned_free(size_t alignment, void *ptr)
{
    currentHeap->aligned_free(currentHeap->data, alignment, ptr);
}

void raft_heap_set(struct raft_heap *heap)
{
    currentHeap = heap;
}

void raft_heap_set_default(void)
{
    currentHeap = &defaultHeap;
}
