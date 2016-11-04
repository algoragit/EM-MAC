#include "net/mac/EM-MAC/Neighbors_list.h"
unsigned int get_neighbor_wake_up_time(neighbor_state v, uint8_t *neighbor_channel, unsigned int time_in_advance, unsigned int *iteration_out);
void generate_ch_list(int *ch_list, int seed, int no_channels);
