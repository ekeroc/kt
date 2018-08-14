/*
 * utility
 *
 *  Created on: 2012/8/13
 *      Author: 990158
 */

#ifndef UTILITY_
#define UTILITY_

#define htonll(x) \
((((x) & 0xff00000000000000LL) >> 56) | \
(((x) & 0x00ff000000000000LL) >> 40) | \
(((x) & 0x0000ff0000000000LL) >> 24) | \
(((x) & 0x000000ff00000000LL) >> 8) | \
(((x) & 0x00000000ff000000LL) << 8) | \
(((x) & 0x0000000000ff0000LL) << 24) | \
(((x) & 0x000000000000ff00LL) << 40) | \
(((x) & 0x00000000000000ffLL) << 56))

#define ntohll(x)	htonll(x)

static inline unsigned long long compose_triple(int rbid, int len, int offset) {
	unsigned long long val = 0;

	val = ((unsigned long long)rbid) << 32 | ((unsigned long long)len) << 20 | offset;
	return val;
}

/*
 * datanode path format: <Mount Point>/<Datanode ID>-<LUN>/<(Local ID & 0xffc00)>>10>/<RBID>-<Local ID>.dat
 *   ex: /usr/cloudos/data/dms/dms-data/lun/carrier3/15-3/0/1009778692-4.dat
 *       <Mount Point> 			: /usr/cloudos/data/dms/dms-data/lun/carrier3
 *       <Datanode ID>-<LUN> 	: 15-3
 *       <(Local ID & 0xffc00)>>10> : 0
 *   	 <RBID>-<Local ID>.dat	: 1009778692-4.dat
 * RBID: DNID 6 bits / LUN 6 bits / LocalID 20 bits
 * RB's base unit: HBID 8 bytes + data-payload 4096 bytes
 */
static inline void decompose_triple(unsigned long long val, int* rbid, int * len,
		int * offset) {
	unsigned long long tmp;
	tmp = val;
	*rbid = (int) (val >> 32);
	tmp <<= 32;
	*len = (int) (tmp >> 32) >> 20;
	*offset = (int) (val & 0x000000000000ffffLL);

}

int Calculate_Num_of_1_in_char(char char_data);

#endif /* UTILITY_ */
