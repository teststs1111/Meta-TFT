#ifndef API_CLIENT_H
#define API_CLIENT_H
#include <stddef.h>
#define API_BASE_URL "https://api-hc.metatft.com/tft-explorer-api/"
#define API_MAX_URL_LEN 512
#define API_UNIT_NAME_LEN 64
typedef struct { char name[API_UNIT_NAME_LEN]; int n; float avg_place; float win_rate; float top4_rate; } RankedEntry;
int api_client_init(void);
void api_client_shutdown(void);
int api_fetch_ranked_list(const char *path,const char *query,const char *name_key,RankedEntry *entries,int max_entries,int *out_count);
void ranked_entries_sort_by_place(RankedEntry *entries,int count);
#endif
