/* warning: this code is not carefully tested. */

#include <stdint.h>
#include <stdio.h>
#include <abi.h>
#include <panic.h>
#include <pair>
#include <vector>
#include <algorithm>

using std::vector;
using std::pair;
using std::make_pair;
using std::sort;

typedef void (* fn_ptr) (void *);

namespace ABI {
	
typedef pair<void *, int> atexit_info_t; // pobj, insert_index

struct atexit_t {
	fn_ptr lpfn;
	vector<atexit_info_t> atexits;
	atexit_t *next;
};

const int slot_size = 131; // prime

static atexit_t *slot_header[slot_size] = {0};
static size_t cur_insert_index;

static uint32_t hash(fn_ptr lpfn) {
	return uint32_t(lpfn) * uint32_t(2654435761);
}

/* we don't need dso_handle actually. */
static int cxa_atexit(fn_ptr lpfn, void *pobj, void * /* dso_handle */) {
	printf("[cxa_atexit] register lpfn = 0x%x, pobj = 0x%x\n", lpfn, pobj);
	
	uint32_t h = hash(lpfn) % slot_size;
	atexit_t *patexit = slot_header[h];
	
	while (patexit != NULL) {
		if (patexit->lpfn == lpfn) {
			break;
		} else {
			patexit = patexit->next;
		}
	}
	
	if (patexit == NULL) {
		patexit = new atexit_t();
		patexit->lpfn = lpfn;
		patexit->next = slot_header[h];
		slot_header[h] = patexit;
	}
	
	patexit->atexits.push_back(make_pair(pobj, ++cur_insert_index));
	
	return 0;
}

static void cxa_finalize(fn_ptr lpfn) {
	if (lpfn != NULL) {
		uint32_t h = hash(lpfn) % slot_size;
		atexit_t *patexit = slot_header[h], *prev = NULL;
		
		while (patexit != NULL) {
			if (patexit->lpfn == lpfn) {
				break;
			} else {
				prev = patexit;
				patexit = patexit->next;
			}
		}
		if (patexit == NULL) {
			/* key lpfn is not found in the table .*/
			panic::panic("[cxa_finalize] Bad function pointer.");
		}
		for (int i = int(patexit->atexits.size()) - 1; i >= 0; --i) {
			patexit->lpfn(patexit->atexits[i].first);
		}
		if (prev == NULL) {
			slot_header[h] = patexit->next;
		} else {
			prev->next = patexit->next;
		}
		delete patexit;
	} else {
		vector<pair<fn_ptr, atexit_info_t>> all;
		for (int i = 0; i < slot_size; ++i) {
			atexit_t *p = slot_header[i];
			while (p != NULL) {
				for (auto &item : p->atexits) {
					all.push_back(make_pair(p->lpfn, item));
				}
				auto backup = p;
				p = p->next;
				delete backup;
			}
		}
		sort(all.begin(), all.end(), [&](const pair<fn_ptr, atexit_info_t> &a, const pair<fn_ptr, atexit_info_t> &b) -> bool {
			return a.second.second > b.second.second; // compared by insert order
		});
		for (auto it = all.begin(); it != all.end(); ++it) {
			it->first(it->second.first);
		}
	}
}

} /* ABI */

/* exports go here. */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void *__dso_handle = 0; 

int __cxa_atexit(fn_ptr lpfn, void *pobj, void *dso_handle) {
	return ABI::cxa_atexit(lpfn, pobj, dso_handle);
}

void __cxa_finalize(fn_ptr lpfn) {
	ABI::cxa_finalize(lpfn);
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

namespace ABI {
	
void dtors(void) {
	__cxa_finalize(NULL);
}

} /* ABI */