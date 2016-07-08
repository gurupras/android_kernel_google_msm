#ifndef __TEMPFREQ_H_
#define __TEMPFREQ_H_


#define MIN_TEMPERATURE		0
#define MAX_TEMPERATURE		100
#define TEMP_FREQUENCY_MS	250

extern struct kobject tempfreq_kobj;

struct temp_list {
	int num_elements;
	int max_elements;
	int temperatures[MAX_TEMPERATURE - MIN_TEMPERATURE];
	struct heap *min_heap;
	struct heap *max_heap;
	struct list_head list;
};

extern struct temp_list *long_temp_list, *short_temp_list;

int get_nth_percentile(struct temp_list *tl, int n);

#endif	/* __TEMPFREQ_H_ */
