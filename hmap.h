#ifndef URANUS_HMAP
#define URANUS_HMAP

#include "3rd/map.h"
#include "3rd/map.c"

#define urn_hmap_init(size)                hashmap_create(size);
#define urn_hmap_free(hmap)                hashmap_free(hmap);
#define urn_hmap_set(hmap, str, slen, ptr) hashmap_set(hmap, str, slen, ptr);
#define urn_hmap_setstr(hmap, str, ptr)    hashmap_set(hmap, str, strlen(str), (uintptr_t)(ptr));
#define urn_hmap_del(hmap, str, slen)      hashmap_remove(hmap, str, slen);
#define urn_hmap_delstr(hmap, str)         hashmap_remove(hmap, str, strlen(str));

/**
 Macro for iterating over an hashmap.
 It works like iterator, but with a more intuitive API.
 
 @par Example
 @code
    size_t idx, klen;
    char *key;
    void *val;
    urn_hmap_foreach(hmap, idx, key, klen, val) {
        your_func(idx, key, klen, val);
    }
 @endcode
 */
#define urn_hmap_foreach(hmap, idx, key, klen, val) \
	hashmap_foreach(hmap, idx, key, klen, val)

int urn_hmap_get() {
	return 0;
}

#endif
