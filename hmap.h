#ifndef URANUS_HMAP
#define URANUS_HMAP

#include "3rd/map.h"
#include "3rd/map.c"

#define urn_hmap_init(size)                hashmap_create(size);
#define urn_hmap_free(hmap)                hashmap_free(hmap);
#define urn_hmap_set(hmap, str, ptr)       hashmap_set(hmap, str, strlen(str), (uintptr_t)(ptr));
#define urn_hmap_setstr(hmap, str, ptr)    hashmap_set(hmap, str, strlen(str), (uintptr_t)(ptr));
#define urn_hmap_del(hmap, str, slen)      hashmap_remove(hmap, str, slen);
#define urn_hmap_delstr(hmap, str)         hashmap_remove(hmap, str, strlen(str));
#define urn_hmap_getptr(hmap, str, ptr)    hashmap_get(hmap, (void *)str, strlen(str), (ptr));
#define urn_hmap_getstr(hmap, str, ptr)    hashmap_get(hmap, (void *)str, strlen(str), (uintptr_t *)(ptr));

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

void urn_hmap_print(hashmap *hmap, char *title) {
	size_t idx, klen;
	char *key;
	void *val;
	URN_LOGF("hmap foreach " URN_BLUE("%s"), title);
	urn_hmap_foreach(hmap, idx, key, klen, val) {
		URN_LOGF("\t%4zu " URN_BLUE("%s") " len(%zu)-> %lu -> " URN_BLUE("%s"),
				idx, key, klen, 
				(unsigned long)val, (char*)val);
	}
	URN_LOGF("         end %s", title);
}
// same as urn_hmap_print() but print val as number
void urn_hmap_printi(hashmap *hmap, char *title) {
	size_t idx, klen;
	char *key;
	void *val;
	URN_LOGF("hmap foreach " URN_BLUE("%s"), title);
	urn_hmap_foreach(hmap, idx, key, klen, val) {
		URN_LOGF("\t%4zu " URN_BLUE("%s") " len(%zu)-> " URN_BLUE("%lu"),
				idx, key, klen, 
				(unsigned long)val);
	}
	URN_LOGF("         end %s", title);
}

#define urn_hmap_free(hmap) hashmap_free(hmap);

void urn_hmap_free_with_keys(hashmap *hmap) {
	size_t idx, klen;
	char *key;
	void *val;
	urn_hmap_foreach(hmap, idx, key, klen, val) {
		free(key);
	}
	hashmap_free(hmap);
}

void urn_hmap_free_with_vals(hashmap *hmap, urn_ptr_cb free_cb) {
	size_t idx, klen;
	char *key;
	void *val;
	urn_hmap_foreach(hmap, idx, key, klen, val) {
		free_cb(val);
	}
	hashmap_free(hmap);
}

void urn_hmap_free_with_keyvals(hashmap *hmap, urn_ptr_cb free_cb) {
	size_t idx, klen;
	char *key;
	void *val;
	urn_hmap_foreach(hmap, idx, key, klen, val) {
		free(key);
		free_cb(val);
	}
	hashmap_free(hmap);
}

#endif
