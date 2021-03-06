/****************************************Copyright (c)****************************************************
**                             成 都 世 纪 华 宁 科 技 有 限 公 司
**                                http://www.huaning-iot.com
**                                http://hichard.taobao.com
**
**
**--------------File Info---------------------------------------------------------------------------------
** File Name:           arch_io_devinfo.h
** Last modified Date:  2017-10-01
** Last Version:        v1.0
** Description:         芯片电气信息获取功能函数实现
**
**--------------------------------------------------------------------------------------------------------
** Created By:          Renhaibo任海波
** Created date:        2017-10-01
** Version:             v1.0
** Descriptions:        The original version 初始版本
**
**--------------------------------------------------------------------------------------------------------
** Modified by:
** Modified date:
** Version:
** Description:
**
*********************************************************************************************************/
#ifndef __ARCH_IO_DEVINFO_H__
#define __ARCH_IO_DEVINFO_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <rthw.h>

/*********************************************************************************************************
** 外部函数的声明
*********************************************************************************************************/
extern void device_info_uid_get(rt_uint32_t *uid);
extern rt_uint32_t device_flash_size_get(void);



#ifdef __cplusplus
    }
#endif      // __cplusplus

#endif // endif of __ARCH_IO_DEVINFO_H__
/*********************************************************************************************************
  END FILE
*********************************************************************************************************/
