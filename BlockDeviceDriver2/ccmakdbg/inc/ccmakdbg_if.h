/*=============================================================================
 * MODULE NAME: CCMA KDBG
 * FILE NAME:   ccmakdbg_if.h
 * DATE:        12/1/2011
 * AUTHOR:  Jeremy REVIEWED by 
 * PURPOSE:     
 *          To provide external interfaces of CCMAKDBG.
 *
 *
 * FUNCTIONS                                                          
 *      CCMAKDBG_IF_Init                - Init this module
 *      CCMAKDBG_IF_Exit                - Exit this module
 * 
 *=============================================================================
 */

#ifndef __CCMAKDBG_IF_H
#define __CCMAKDBG_IF_H

#include "../inc/ccmakdbg_err.h"

CCMAKDBG_ERR_CODE_T CCMAKDBG_IF_Init(void);
CCMAKDBG_ERR_CODE_T CCMAKDBG_IF_Exit(void);
#endif
