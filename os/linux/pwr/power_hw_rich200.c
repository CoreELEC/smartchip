#include "wf_debug.h"
#include "hif.h"

#define DXX0_EN_ADDR     		0x00E4
#define DXX0_START_ADDR 		0x00E8
#define DXX0_CLOCK_EN   		0x0094

int side_road_cfg(struct hif_node_ *node)
{
    int ret = 0;
    wf_u8  value8;
    wf_u16 value16;
    wf_u32 value32;

    /************************func configure*****************************/
    
    /* enable reg r/w */
    LOG_I("old:0xac---0x%x",hif_io_read8(node, 0xac,NULL));
    value8 = hif_io_read8(node, 0xac,NULL);
    value8 |= 0x02;
    ret = hif_io_write8(node, 0xac, value8);
    LOG_I("new:0xac---0x%x",hif_io_read8(node, 0xac,NULL));

    /* M0 Uart/Fw_type select */
    LOG_I("old:0xf8---0x%x",hif_io_read8(node, 0xf8,NULL));
    value8 = hif_io_read8(node, 0xf8,NULL);
    value8 |= 0x10;      // special write for all
    value8 |= 0x40;      // ��M0�Ĵ���

#ifdef CONFIG_FW_ENCRYPT
    value8 |= 0x80;      // ���ܹ̼�
#else
    value8 &= 0x7F;      // �Ǽ��ܹ̼�
#endif
    ret = hif_io_write8(node, 0xf8, value8);
    LOG_I("new:0xf8---0x%x",hif_io_read8(node, 0xf8,NULL));

    LOG_I("old:0x98---0x%x",hif_io_read8(node, 0x98,NULL));
    hif_io_write8(node,0x98,0xff);
    LOG_I("new:0x98---0x%x",hif_io_read8(node, 0x98,NULL));


    /*  For Bluk transport */
    #if 1
    ret=hif_io_write32(node, 0x200, 0x00100000);
    LOG_I("0x200---0x%x",hif_io_read32(node, 0x200,NULL));
    ret=hif_io_write32(node, 0x200, 0x80100000);
    LOG_I("0x200---0x%x",hif_io_read32(node, 0x200,NULL));
    #endif

    LOG_I("[%s] cfg sucess",__func__);

    return 0;
}

int power_on(struct hif_node_ *node)
{
	int ret = 0;
    wf_bool initSuccess=wf_false;
    wf_u8  value8;
    wf_u16 value16;
    wf_u32 value32;
    
    LOG_I("[%s] start",__func__);
   // return -1;

    if(HIF_SDIO == node->hif_type)
    {
        // clear sdio suspend
        value8 = hif_io_read8(node, 0x903a,NULL);
        value8 &= 0xFE;
        ret = hif_io_write8(node, 0x903a, value8);
        if( WF_RETURN_FAIL == ret)
        {
            LOG_E("[%s] 0x903a failed, check!!!",__func__);
            return ret;
        }
        
        value16 = 0;
        while(1) {
            value8 = hif_io_read8(node, 0x903a,NULL);
            if(value8 & WF_BIT(1)) {
                break;
            }
            wf_msleep(1);
            value16++;
            if(value16 > 100) {
                break;
            }
        }
        if(value16 > 100) {
            LOG_E("[%s] failed!!!",__func__);
            return WF_RETURN_FAIL;
        }
        LOG_I("[%s]  sdio clear suspend ",__func__);
    }
	
    //set 0x_00AC  bit 4 д0
    value8 = hif_io_read8(node, 0xac,NULL);
    value8 &= 0xEF;
    ret = hif_io_write8(node, 0xac, value8);
    if( WF_RETURN_FAIL == ret)
    {
        LOG_E("[%s] 0xac failed, check!!!",__func__);
        return ret;
    }
    //set 0x_00AC  bit 0 д0
    value8 &= 0xFE;
    ret = hif_io_write8(node, 0xac, value8);
    if( WF_RETURN_FAIL == ret)
    {
        LOG_E("[%s] 0xac failed, check!!!",__func__);
        return ret;
    }
    //set 0x_00AC  bit 0 д1
    value8 |= 0x01;
    ret = hif_io_write8(node, 0xac, value8);
    if( WF_RETURN_FAIL == ret)
    {
        LOG_E("[%s] 0xac failed, check!!!",__func__);
        return ret;
    }
    wf_msleep(10);
    // waiting for power on
    value16 = 0;
  
	while(1){
        value8 = hif_io_read8(node, 0xac,NULL);
        if(value8 & 0x10) {
            initSuccess = wf_true;
            break;
        }
        value16++;
        if(value16 > 1000) {
            break;
        }
    }

    // enable mcu-bus clk
    hif_io_read32(node,0x94,NULL);       
    ret = hif_io_write32(node,0x94,0x6);
    if (WF_RETURN_FAIL == ret)
    {
        LOG_E("[%s] WF_CLK_ADDR failed,check!!!",__func__);
        return WF_RETURN_FAIL;
    }

    if(initSuccess == wf_false) 
    {
        LOG_E("[%s] failed!!!",__func__);
        return WF_RETURN_FAIL;
    }

    LOG_I("[%s] success",__func__);
    
    return WF_RETURN_OK;

}



int power_off(struct hif_node_ *node)
{
	int ret = 0;
	if(hm_get_mod_removed() == wf_false && node->dev_removed == wf_true)
	{
		return 0;
	}

    // disable mcu-bus clk
    hif_io_read32(node,0x94,NULL);
	ret = hif_io_write32(node,0x94,0);
    if (WF_RETURN_FAIL == ret)
    {
    	LOG_E("[%s] WF_CLK_ADDR failed,check!!!",__func__);
        return WF_RETURN_FAIL;
    }
    
	switch(node->hif_type)
    {
        case HIF_USB:
		{
			wf_u8  value8;
            wf_u16 value16;
			wf_u32 value32;

            // 0x00ac  bit 22 write 1, reset dsp
			value32 = hif_io_read32(node, 0xac,NULL);
			value32 |= WF_BIT(22);
			ret = hif_io_write32(node, 0xac, value32);
			if( WF_RETURN_FAIL == ret)
			{
				LOG_E("[%s] 0xac failed, check!!!",__func__);
				return ret;
			}

            // clear the power off bit
			value32 &= ~((wf_u32)WF_BIT(11)); 
			ret = hif_io_write32(node, 0xac, value32);
			if( WF_RETURN_FAIL == ret)
			{
				LOG_E("[%s] 0xac failed, check!!!",__func__);
				return ret;
			}

			// �����µ���ʼ����Ϊ��0x00AC[10] ����0����1��������ʹ��Ӳ���µ�״̬��
			value32 &= ~((wf_u32)WF_BIT(10));  
			ret = hif_io_write32(node, 0xac, value32);
			if( WF_RETURN_FAIL == ret)
			{
				LOG_E("[%s] 0xac failed, check!!!",__func__);
				return ret;
			}
			value32 |= WF_BIT(10);  
			ret = hif_io_write32(node, 0xac, value32);
			if( WF_RETURN_FAIL == ret)
			{
				LOG_E("[%s] 0xac failed, check!!!",__func__);
				return ret;
			}

            wf_msleep(10);
			// waiting for power off
			value16 = 0;
            while(1) {
				value32 = hif_io_read32(node, 0xac,NULL);
				if(value32 & WF_BIT(11)) {
					break;
				}
                wf_msleep(1);
				value16++;
				if(value16 > 10) {
					break;
				}
			}
			if(value16 > 10) {
				LOG_E("[%s] failed!!!",__func__);
				return WF_RETURN_FAIL;
			}
			return WF_RETURN_OK;
		}
		case HIF_SDIO:
		{
            wf_u8  value8;
			wf_u16 value16;
			wf_u32 value32;

            // 0x00ac  bit 22 write 1, reset dsp
            value8 = hif_io_read8(node, 0xac+2,NULL);
			value8 |= WF_BIT(6);
            ret = hif_io_write8(node, 0xac+2, value8);
			if( WF_RETURN_FAIL == ret)
			{
				LOG_E("[%s] 0xac failed, check!!!",__func__);
				return ret;
			}

            // clear the power off bit
			value8 = hif_io_read8(node, 0x9094,NULL);
			value8 &= 0xFE;
			ret = hif_io_write8(node, 0x9094, value8);
			if( WF_RETURN_FAIL == ret)
			{
				LOG_E("[%s] 0x9094 failed, check!!!",__func__);
				return ret;
			}

			// �����µ���ʼ����Ϊ��0x00AC[10] ����0����1��������ʹ��Ӳ���µ�״̬��
            value8 = hif_io_read8(node, 0xac+1,NULL);
            value8 &= ~(WF_BIT(2));
            ret = hif_io_write8(node, 0xac+1, value8);
			if( WF_RETURN_FAIL == ret)
			{
				LOG_E("[%s] 0xac failed, check!!!",__func__);
				return ret;
			}
            value8 |= WF_BIT(2);
            ret = hif_io_write8(node, 0xac+1, value8);
			if( WF_RETURN_FAIL == ret)
			{
				LOG_E("[%s] 0xac failed, check!!!",__func__);
				return ret;
			}
			wf_msleep(10);
			// waiting for power off
			value16 = 0;
            while(1) {
				value8 = hif_io_read8(node, 0x9094,NULL);
				if(value8 & WF_BIT(0)) {
					break;
				}
                wf_msleep(1);
				value16++;
				if(value16 > 100) {
					break;
				}
			}
			if(value16 > 100) {
				LOG_E("[%s] failed!!!",__func__);
				return WF_RETURN_FAIL;
			}

			return WF_RETURN_OK;
		}

		default:
		{
            LOG_E("Error Nic type");
            return WF_RETURN_FAIL;
		}
	}

    return WF_RETURN_OK;
}



