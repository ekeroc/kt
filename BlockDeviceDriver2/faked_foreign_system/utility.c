/*
 * utility.c
 *
 *  Created on: 2012/8/16
 *      Author: 990158
 */
#include "utility.h"

int digit2NumOfOne_array[] = {0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4};

int Calculate_Num_of_1_in_char(char char_data) {
	return digit2NumOfOne_array[char_data & 0xf] + digit2NumOfOne_array[(char_data & 0xf0) >> 4];
}
