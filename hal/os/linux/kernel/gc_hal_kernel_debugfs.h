/****************************************************************************
*
*    Copyright (c) 2005 - 2013 by Vivante Corp.  All rights reserved.
*
*    The material in this file is confidential and contains trade secrets
*    of Vivante Corporation. This is proprietary information owned by
*    Vivante Corporation. No part of this work may be disclosed,
*    reproduced, copied, transmitted, or used in any way for any purpose,
*    without the express written permission of Vivante Corporation.
*
*****************************************************************************/


#include <stdarg.h>

#ifndef __gc_hal_kernel_debugfs_h_
#define __gc_hal_kernel_debugfs_h_

 #define MAX_LINE_SIZE 768           /* Max bytes for a line of debug info */


 typedef struct _gcsDEBUGFS_Node gcsDEBUGFS_Node;


/*******************************************************************************
 **
 **                             System Related
 **
 *******************************************************************************/

gctINT gckDEBUGFS_IsEnabled(void);

gctINT gckDEBUGFS_Initialize(void);

gctINT gckDEBUGFS_Terminate(void);


/*******************************************************************************
 **
 **                             Node Related
 **
 *******************************************************************************/

gctINT
gckDEBUGFS_CreateNode(
    IN gctPOINTER Device,
    IN gctINT SizeInKB,
    IN gctCONST_STRING ParentName,
    IN gctCONST_STRING NodeName,
    OUT gcsDEBUGFS_Node **Node
    );

void gckDEBUGFS_FreeNode(
            IN gcsDEBUGFS_Node  * Node
            );



void gckDEBUGFS_SetCurrentNode(
            IN gcsDEBUGFS_Node  * Node
            );



void gckDEBUGFS_GetCurrentNode(
            OUT gcsDEBUGFS_Node  ** Node
            );


ssize_t gckDEBUGFS_Print(
                IN gctCONST_STRING  Message,
                ...
                );

#endif


