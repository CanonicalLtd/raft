/* Internal heap APIs. */

#ifndef HEAP_H_
#define HEAP_H_

#include <stddef.h>

void *MyHeapMalloc(size_t size);

void *MyHeapCalloc(size_t nmemb, size_t size);

void *MyHeapRealloc(void *ptr, size_t size);

void MyHeapFree(void *ptr);

#endif /* HEAP_H_ */
