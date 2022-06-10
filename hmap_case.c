#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hmap.h"

int main() {
	hashmap* hmap = urn_hmap_init(20);
	urn_hmap_setstr(hmap, "key1", "val1");
	urn_hmap_setstr(hmap, "key2", "val2");
	urn_hmap_setstr(hmap, "key3", "val2");
	urn_hmap_setstr(hmap, "key4", "val2");
	urn_hmap_delstr(hmap, "key3");

	size_t idx, klen;
	char *key;
	void *val;
	printf("hmap foreach\n");
	urn_hmap_foreach(hmap, idx, key, klen, val) {
		printf("hmap %zu %s (%zu)-> %s\n", idx, key, klen, (char *)val);
	}
	printf("hmap foreach done\n");
	return 0;
}
