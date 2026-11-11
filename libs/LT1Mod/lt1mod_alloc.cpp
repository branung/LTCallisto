// stdlith's allocator hooks, for modules that have none. NOLF2's modules define their own, and
// Shogo's and Blood II's never linked stdlith. Alone in this object file on purpose: a static
// library contributes it only when the symbols are still unresolved, so a module with its own
// pair never pulls this one in.

#include <stdlib.h>

#include "l_allocator.h"

void* DefStdlithAlloc(uint32 size)
{
	if (size == 0)
		return NULL;
	return malloc((size_t)size);
}

void DefStdlithFree(void* ptr)
{
	if (ptr)
		free(ptr);
}
