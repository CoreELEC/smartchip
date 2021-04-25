#include "common.h"
#include "wf_debug.h"

/* macro */
#if 1
#define ACTION_DBG(fmt, ...)        LOG_D("[%s]"fmt, __func__, ##__VA_ARGS__)
#define ACTION_ARRAY(data, len)     log_array(data, len)
#else
#define ACTION_DBG(fmt, ...)
#define ACTION_ARRAY(data, len)
#endif
#define ACTION_WARN(fmt, ...)       LOG_E("[%s]"fmt, __func__, ##__VA_ARGS__)
#define ACTION_INFO(fmt, ...)       LOG_I("[%s]"fmt, __func__, ##__VA_ARGS__)

static
wf_u8 get_rx_ampdu_size(nic_info_st *nic_info)
{
    wf_u8 size;
    hw_info_st *hw_info = nic_info->hw_info;

    switch (hw_info->max_rx_ampdu_factor)
    {
        case MAX_AMPDU_FACTOR_64K:
            size = 64;
            break;
        case MAX_AMPDU_FACTOR_32K:
            size = 32;
            break;
        case MAX_AMPDU_FACTOR_16K:
            size = 16;
            break;
        case MAX_AMPDU_FACTOR_8K:
            size = 8;
            break;
        default:
            size = 64;
            break;
    }

    return size;
}

static
int action_frame_add_ba_response(nic_info_st *nic_info)
{
    mlme_info_t *pmlme_info = nic_info->mlme_info;
    wf_add_ba_parm_st *barsp_parm = &pmlme_info->barsp_parm;
    int rst;

    barsp_parm->size = get_rx_ampdu_size(nic_info);
    rst = wf_mlme_add_ba_rsp(nic_info);
    if (rst)
    {
        ACTION_WARN("wf_mlme_add_ba_rsp fail, error code: %d", rst);
        return -1;
    }

    return 0;
}

static
int action_frame_block_ack (nic_info_st *nic_info, wdn_net_info_st *pwdn_info,
                            wf_u8 *pkt, wf_u16 pkt_len)
{
    wf_80211_mgmt_t *pmgmt = (wf_80211_mgmt_t *)pkt;
    wf_u16 status, tid, reason_code = 0;
    wf_u8 *frame_body;
    wf_u8 dialog, policy, size;
    struct ADDBA_request *preq;
    wf_u8 action;
    wf_u16 param, start_req;
    mlme_info_t *mlme_info = nic_info->mlme_info;
    wf_add_ba_parm_st *barsp_parm = &mlme_info->barsp_parm;

    frame_body = &pmgmt->action.variable[0];
    action = pmgmt->action.action_field;
    if (pwdn_info == NULL)
    {
        return -1;
    }

    switch (action)
    {
        case WF_WLAN_ACTION_ADDBA_REQ:
        {
            frame_body = &pmgmt->action.variable[0];
            preq = ( struct ADDBA_request *)frame_body;
            barsp_parm->dialog = preq->dialog_token;
            param = wf_le16_to_cpu(preq->BA_para_set);
            barsp_parm->param = param;
            barsp_parm->tid = (param & 0x3c) >> 2;
            barsp_parm->policy = (param & 0x2) >> 1;
            barsp_parm->size = (param & (~0xe03f)) >> 6;

            barsp_parm->timeout = wf_le16_to_cpu(preq->BA_timeout_value);
            barsp_parm->start_seq = wf_le16_to_cpu(preq->ba_starting_seqctrl) >> 4;
            barsp_parm->status = 0;

            LOG_D("WF_WLAN_ACTION_ADDBA_REQ TID:%d dialog:%d size:%d policy:%d start_req:%d timeout:%d",
                  barsp_parm->tid, barsp_parm->dialog, barsp_parm->size, barsp_parm->policy, barsp_parm->start_seq, barsp_parm->timeout);

            action_frame_add_ba_response(nic_info);
        }
        break;

        case WF_WLAN_ACTION_ADDBA_RESP:
        {
            status = WF_GET_LE16(&frame_body[1]);
            tid = ((frame_body[3] >> 2) & 0x7);
            if (status == 0)
            {
                pwdn_info->htpriv.agg_enable_bitmap |= 1 << tid;
                pwdn_info->htpriv.candidate_tid_bitmap &= ~(BIT(tid));

                if (frame_body[3] & 1)
                    pwdn_info->htpriv.tx_amsdu_enable = wf_true;
            }
            else
            {
                pwdn_info->htpriv.agg_enable_bitmap &= ~(BIT(tid));
            }

            mlme_info->baCreating = 0;

            LOG_D("WF_WLAN_ACTION_ADDBA_RESP status:%d tid:%d  (agg_enable_bitmap:%d candidate_tid_bitmap:%d)",
                  status, tid, pwdn_info->htpriv.agg_enable_bitmap, pwdn_info->htpriv.candidate_tid_bitmap);

        }
        break;

        case WF_WLAN_ACTION_DELBA:
        {
            if ((frame_body[0] & BIT(3)) == 0)
            {
                tid = (frame_body[0] >> 4) & 0x0F;

                pwdn_info->htpriv.agg_enable_bitmap &= ~(1 << tid);
                pwdn_info->htpriv.candidate_tid_bitmap &= ~(1 << tid);

                reason_code = WF_GET_LE16(&frame_body[2]);
            }
            else if ((frame_body[0] & BIT(3)) == BIT(3))
            {
                tid = (frame_body[0] >> 4) & 0x0F;
            }


            LOG_D("WF_WLAN_ACTION_DELBA reason_code:%d tid:%d", reason_code, tid);
        }
        break;
        default:
            break;
    }

    return 0;
}

static
void action_frame_wlan_hdr (nic_info_st *pnic_info, struct xmit_buf *pxmit_buf)
{
    wf_u8 *pframe;
    struct wl_ieee80211_hdr *pwlanhdr;
    mlme_info_t *pmlme_info = (mlme_info_t *)pnic_info->mlme_info;;
    //LOG_D("[action]%s",__func__);
    pframe = pxmit_buf->pbuf + TXDESC_OFFSET;
    pwlanhdr = (struct wl_ieee80211_hdr *)pframe;

    pwlanhdr->frame_ctl = 0;
    SetFrameType(pframe, WIFI_MGT_TYPE);
    SetFrameSubType(pframe, WIFI_ACTION);  /* set subtype */

}

#if 0
static
int action_frame_channel_switch (nic_info_st *nic_info, wf_u8 *pdata, wf_u16 pkt_len)
{
    wf_80211_mgmt_t *pmgmt = (wf_80211_mgmt_t *)pdata;
    wf_80211_mgmt_ie_t *pie;
    wdn_net_info_st *pwdn_info;
    wf_u8 *pele_start;
    wf_u8 *pele_end;
    wf_u8 *frame_body;
    wf_u8 switch_mode = -1, new_number = -1, switch_count = -1, switch_offset = -1;
    wf_u8 bwmode;
    int rst = 0;
    LOG_D("[action]%s", __func__);

    pwdn_info = wf_wdn_find_info(nic_info, wf_wlan_get_cur_bssid(nic_info));
    if (pwdn_info == NULL)
    {
        return -1;
    }
    pele_start = &pmgmt->action.variable[0];
    pele_end = &pele_start[pkt_len - WF_OFFSETOF(wf_80211_mgmt_t, action.variable)];
    do
    {
        pie = (wf_80211_mgmt_ie_t *)pele_start;
        frame_body = &pie->data[0];
        switch (pie->element_id)
        {
            case WF_80211_MGMT_EID_CHANNEL_SWITCH:
            {
                switch_mode = frame_body[0];
                new_number = frame_body[1];
                switch_count = frame_body[2];
            }
            break;
            case WF_80211_MGMT_EID_SECONDARY_CHANNEL_OFFSET:
            {
                switch_offset = frame_body[0];
            }
            default:
                break;
        }

        pele_start = &pele_start[WF_OFFSETOF(wf_80211_mgmt_ie_t, data) + pie->len];

    }
    while (pele_start < pele_end);
    bwmode = (switch_offset == HAL_PRIME_CHNL_OFFSET_DONT_CARE) ?
             CHANNEL_WIDTH_20 : CHANNEL_WIDTH_40;
    rst = wf_hw_info_set_channnel_bw(nic_info, new_number, bwmode, switch_offset);
    if (rst == 0)
    {
        pwdn_info->bw_mode = bwmode;
        pwdn_info->channel = new_number;
    }
    return rst;
}

int wf_action_frame_spectrum(nic_info_st *nic_info, wf_u8 *pdata, wf_u16 pkt_len)
{
    wf_80211_mgmt_t *pmgmt = (wf_80211_mgmt_t *)pdata;
    wf_u8 *frame_body;
    wf_u8 action;

    LOG_D("[action]%s", __func__);
    frame_body = &pmgmt->action.variable[0];
    action = pmgmt->action.action_field;
    switch (action)
    {
        case WF_WLAN_ACTION_SPCT_CHL_SWITCH:
        {
            action_frame_channel_switch(nic_info, pdata, pkt_len);
        }
        break;
        default:
            break;
    }
    return 0;
}

int wf_action_frame_bsscoex(nic_info_st *nic_info, wf_u8 *pdata, wf_u16 pkt_len)
{
    wf_80211_mgmt_t *pmgmt = (wf_80211_mgmt_t *)pdata;
    wf_80211_mgmt_ie_t *pie;
    wf_u8 *frame_body;
    wf_u8 *pele_start;
    wf_u8 *pele_end;
    wf_u32 frame_body_len;
    wdn_net_info_st *pwdn_info;
    LOG_D("[action]%s", __func__);


    pwdn_info = wf_wdn_find_info(nic_info, pmgmt->bssid);
    pele_start = &pmgmt->action.variable[0];
    pele_end = &pele_start[pkt_len - WF_OFFSETOF(wf_80211_mgmt_t, action.variable)];

    /*ap songqiang add*/
//  do{
//      pie = (wf_80211_mgmt_ie_t *)pele_start;
//      switch(pie->element_id){
//      case WF_80211_MGMT_EID_BSS_COEX_2040:
//      {
//          if (pie->data & WF_WLAN_20_40_BSS_COEX_40MHZ_INTOL) {
//              if (psta->ht_40mhz_intolerant == 0) {
//                  psta->ht_40mhz_intolerant = 1;
//                  pmlmepriv->num_sta_40mhz_intolerant++;
//                  beacon_updated = _TRUE;
//              }
//          } else if (pie->data  & WF_WLAN_20_40_BSS_COEX_20MHZ_WIDTH_REQ) {
//              if (pmlmepriv->ht_20mhz_width_req == _FALSE) {
//                  pmlmepriv->ht_20mhz_width_req = _TRUE;
//                  beacon_updated = _TRUE;
//              }
//          } else
//              beacon_updated = _FALSE;
//      }
//      }break;
//      case WF_80211_MGMT_EID_BSS_INTOLERANT_CHL_REPORT:
//      {

//          if (pmlmepriv->ht_intolerant_ch_reported == _FALSE) {
//              pmlmepriv->ht_intolerant_ch_reported = _TRUE;
//              beacon_updated = _TRUE;
//          }

//      }break;
//      default :
//      break;
//      }
//  pele_start = &pele_start[WF_OFFSETOF(wf_80211_mgmt_ie_t,data)+pie->len];
//  }while(pele_start < pele_end);

    return 0;
}

int wf_action_frame_public(nic_info_st *nic_info, wf_u8 *pdata, wf_u16 pkt_len)
{
    wf_80211_mgmt_t *pmgmt = (wf_80211_mgmt_t *)pdata;
    wf_u8 *frame_body;
    mlme_info_t *mlme_info = (mlme_info_t *)nic_info->mlme_info;
    LOG_D("[action]%s", __func__);

    frame_body = &pmgmt->action.variable[0];
    /*sta don't need songqiang add*/
//  if(pmgmt->action.action_field == WF_WLAN_ACTION_PUBLIC_BSSCOEXIST){
//      if(mlme_info->state & WIFI_FW_AP_STATE){
//          wf_action_frame_bsscoex(nic_info,pdata,pkt_len);
//      }

//  }else if(pmgmt->action.action_field == WF_WLAN_ACTION_PUBLIC_VENDOR){

//  }else{


//  }

    return 0;

}

int wf_action_frame_ht(nic_info_st *nic_info, wf_u8 *pdata, wf_u16 pkt_len)
{
    LOG_D("[action]%s", __func__);
    return 0;
}
#endif

int wf_action_frame_process (nic_info_st *nic_info, wdn_net_info_st *pwdn_info,
                             wf_80211_mgmt_t *pmgmt, wf_u16 mgmt_len)
{
    mlme_info_t *mlme_info = (mlme_info_t *)nic_info->mlme_info;
    wf_u8 category;

    category = pmgmt->action.action_category;

    if ((mlme_info->state & 0x03) != WIFI_FW_AP_STATE)
    {
        if (mlme_info->connect == wf_false)
            return 0;
    }

    switch (category)
    {
        case WF_WLAN_CATEGORY_BACK:
        {
            hw_info_st *hw_info = nic_info->hw_info;
            if (hw_info->ba_enable == wf_true)
            {
                action_frame_block_ack(nic_info, pwdn_info, (wf_u8 *)pmgmt, mgmt_len);
            }
        }
        break;
        case WF_WLAN_CATEGORY_SPECTRUM_MGMT:
        {
            //wf_action_frame_spectrum(nic_info, pkt, pkt_len);
        }
        break;
        case WF_WLAN_CATEGORY_PUBLIC:
        {
            //wf_action_frame_public(nic_info,pkt,pkt_len);
        }
        break;
        case WF_WLAN_CATEGORY_HT:
        {
            //wf_action_frame_ht(nic_info,pkt,pkt_len);
        }
        break;
        case WF_WLAN_CATEGORY_SA_QUERY:
        {

        }
        break;
        case WF_WLAN_CATEGORY_P2P:
        {

        }
        break;
        default:
        {

        }
        break;
    }

    return 0;

}

int wf_action_frame_ba_to_issue (nic_info_st *nic_info, wf_u8 action)
{
    int rst = 0;
    wf_u8 *pframe;
    wf_u16 ba_para_set;
    wf_u16 ba_timeout_value;
    wf_u16 ba_starting_seqctrl;
    wf_u16 start_seq;
    struct wl_ieee80211_hdr *pwlanhdr;
    struct xmit_buf *pxmit_buf;
    wf_u32 pkt_len, i;
    tx_info_st     *ptx_info;
    wdn_net_info_st *pwdn_info;
    mlme_info_t *mlme_info;
    hw_info_st *hw_info = (hw_info_st *)nic_info->hw_info;
    wf_u8 initiator = 0;
    wf_u8 category = WF_WLAN_CATEGORY_BACK;
    wf_add_ba_parm_st *barsp_info;
    wf_add_ba_parm_st *bareq_info;


    //LOG_D("[action]%s",__func__);

    pwdn_info = wf_wdn_find_info(nic_info, wf_wlan_get_cur_bssid(nic_info));
    if (pwdn_info == NULL)
    {
        return -1;
    }

    ptx_info =  (tx_info_st *)nic_info->tx_info;
    mlme_info = (mlme_info_t *)nic_info->mlme_info;
    barsp_info = &mlme_info->barsp_parm;
    bareq_info = &mlme_info->bareq_parm;

    /* alloc xmit_buf */
    pxmit_buf = wf_xmit_extbuf_new(ptx_info);
    if (pxmit_buf == NULL)
    {
        LOG_E("[%s]: pxmit_buf is NULL", __func__);
        return -1;
    }
    wf_memset(pxmit_buf->pbuf, 0, WLANHDR_OFFSET + TXDESC_OFFSET);

    /* type of management is 0100 */
    action_frame_wlan_hdr(nic_info, pxmit_buf);

    /* set txd at tx module */
    pframe = pxmit_buf->pbuf + TXDESC_OFFSET; /* pframe point to wlan_hdr */
    pwlanhdr = (struct wl_ieee80211_hdr *)pframe;

    /* copy addr1/2/3 */
    wf_memcpy(pwlanhdr->addr1, pwdn_info->mac, MAC_ADDR_LEN);
    wf_memcpy(pwlanhdr->addr2, nic_to_local_addr(nic_info), MAC_ADDR_LEN);
    wf_memcpy(pwlanhdr->addr3, pwdn_info->bssid, MAC_ADDR_LEN);

    pkt_len = sizeof(struct wl_ieee80211_hdr_3addr);
    pframe += pkt_len; /* point to iv or frame body */

    pframe = set_fixed_ie(pframe, 1, &(category), &pkt_len);
    pframe = set_fixed_ie(pframe, 1, &(action), &pkt_len);

    switch (action)
    {

        case WF_WLAN_ACTION_ADDBA_RESP:
        {
            pframe = set_fixed_ie(pframe, 1, &(barsp_info->dialog), &pkt_len);
            pframe = set_fixed_ie(pframe, 2, (wf_u8 *) & (barsp_info->status), &pkt_len);

            ba_para_set = barsp_info->param;
            ba_para_set &= ~IEEE80211_ADDBA_PARAM_TID_MASK;
            ba_para_set |= (barsp_info->tid << 2) & IEEE80211_ADDBA_PARAM_TID_MASK;
            ba_para_set &= ~IEEE80211_ADDBA_PARAM_BUF_SIZE_MASK;
            ba_para_set |= (barsp_info->size << 6) & IEEE80211_ADDBA_PARAM_BUF_SIZE_MASK;
            ba_para_set &= ~(BIT(0));
            ba_para_set = wf_cpu_to_le16(ba_para_set);

            pframe = set_fixed_ie(pframe, 2, (wf_u8 *)(&(ba_para_set)), &pkt_len);
            pframe = set_fixed_ie(pframe, 2, (wf_u8 *)(&(barsp_info->timeout)), &pkt_len);

            LOG_I("[action response] dialog:%d  ba_para_set:0x%x  timeout:%d  status:%d", barsp_info->dialog,
                  ba_para_set, barsp_info->timeout, barsp_info->status);
        }
        break;

        case WF_WLAN_ACTION_ADDBA_REQ:
        {
            wf_u8 dialog;

            mlme_info->baCreating = 1;

            dialog = pwdn_info->dialogToken[bareq_info->tid] + 1;
            if (dialog > 7)
            {
                dialog = 1;
            }

            pwdn_info->dialogToken[bareq_info->tid] = dialog;
            pframe = set_fixed_ie(pframe, 1, &(dialog), &pkt_len);

            ba_para_set = (0x1002 | ((bareq_info->tid & 0xf) << 2));
            ba_para_set = wf_cpu_to_le16(ba_para_set);
            pframe = set_fixed_ie(pframe, 2, (wf_u8 *)(&(ba_para_set)), &pkt_len);

            ba_timeout_value = 5000;
            ba_timeout_value = wf_cpu_to_le16(ba_timeout_value);
            pframe = set_fixed_ie(pframe, 2, (wf_u8 *)(&(ba_timeout_value)), &pkt_len);

            if (pwdn_info != NULL)
            {
                start_seq = (pwdn_info->wdn_xmitpriv.txseq_tid[bareq_info->tid] & 0xfff) + 1;

                pwdn_info->ba_starting_seqctrl[bareq_info->tid] = start_seq;
                ba_starting_seqctrl = start_seq << 4;
            }

            ba_starting_seqctrl = wf_cpu_to_le16(ba_starting_seqctrl);
            pframe = set_fixed_ie(pframe, 2, (unsigned char *)(&(ba_starting_seqctrl)), &pkt_len);
            LOG_I("[action request] TID:%d  dialog:%d  ba_para_set:0x%x  start_req:%d", bareq_info->tid, dialog, ba_para_set, start_seq);
        }
        break;

        case WF_WLAN_ACTION_DELBA:
            ba_para_set = 0;
            ba_para_set |= (barsp_info->tid << 12) & IEEE80211_DELBA_PARAM_TID_MASK;
            ba_para_set |= (initiator << 11) & IEEE80211_DELBA_PARAM_INITIATOR_MASK;

            ba_para_set = wf_cpu_to_le16(ba_para_set);
            pframe = set_fixed_ie(pframe, 2, (unsigned char *)(&(ba_para_set)), &pkt_len);
            barsp_info->status = wf_cpu_to_le16(barsp_info->status);
            pframe = set_fixed_ie(pframe, 2, (unsigned char *)(&(barsp_info->status)), &pkt_len);

            LOG_D("[action delete] reason:%d  ba_para_set:0x%x", barsp_info->status, ba_para_set);
            break;
        default:
            break;

    }
    pxmit_buf->pkt_len = pkt_len;

    rst = wf_nic_mgmt_frame_xmit_with_ack(nic_info, pwdn_info, pxmit_buf, pxmit_buf->pkt_len);
    //rst = wf_nic_mgmt_frame_xmit(nic_info, pwdn_info, pxmit_buf,pxmit_buf->pkt_len);

    return rst;
}

int wf_action_frame_add_ba_request(nic_info_st *nic_info, struct xmit_frame *pxmitframe)
{
    wf_u8 *mem_addr,issued;
    wf_u32 ff_hwaddr;
    wf_bool rst = wf_true;
    wf_bool inner_ret = wf_true;
    wf_bool blast;
    int t, sz, w_sz, pull = 0;
    struct xmit_buf *pxmitbuf = pxmitframe->pxmitbuf;
    mlme_info_t *mlme_info;
    wdn_net_info_st *pwdn_info;

    if (pxmitframe->bmcast)
    {
        return -1;
    }

    mlme_info = (mlme_info_t *)nic_info->mlme_info;

    pwdn_info = pxmitframe->pwdn;
    if (pwdn_info == NULL)
    {
        return -1;
    }

    if (pwdn_info->ba_enable_flag[pxmitframe->priority] == wf_true)
    {
        return -1;
    }

    if ((pwdn_info->htpriv.ht_option == wf_true) && (pwdn_info->htpriv.ampdu_enable == wf_true))
    {
        issued = (pwdn_info->htpriv.agg_enable_bitmap >> pxmitframe->priority) & 0x1;
        issued |= (pwdn_info->htpriv.candidate_tid_bitmap >> pxmitframe->priority) & 0x1;
        if(issued == 0)
        {
            if ((pxmitframe->frame_tag == DATA_FRAMETAG) && (pxmitframe->ether_type != 0x0806) &&
                (pxmitframe->ether_type != 0x888e) && (pxmitframe->dhcp_pkt != 1))
            {
                pwdn_info->htpriv.candidate_tid_bitmap |= WF_BIT(pxmitframe->priority);
                mlme_info->bareq_parm.tid = pxmitframe->priority;
                pwdn_info->ba_enable_flag[pxmitframe->priority] = wf_true;
                wf_mlme_add_ba_req(nic_info);
                return 0;
            }
        }
    }

    return -1;
}

int wf_action_frame_del_ba_request(nic_info_st *nic_info)
{
    wdn_net_info_st *wdn_net_info;
    mlme_info_t *mlme_info = (mlme_info_t *)nic_info->mlme_info;

    wdn_net_info  = wf_wdn_find_info(nic_info, wf_wlan_get_cur_bssid(nic_info));
    if (NULL != wdn_net_info)
    {
        if (wf_false == wf_wdn_is_alive(wdn_net_info, 1))
        {
            unsigned int tid;
            for (tid = 0; tid < TID_NUM; tid++)
            {
                if (wdn_net_info->ba_started_flag[tid] == 1)
                {
                    mlme_info->barsp_parm.tid = tid;
                    wf_action_frame_ba_to_issue(nic_info, WF_WLAN_ACTION_DELBA);

                    wdn_net_info->ba_started_flag[tid] = wf_false;
                    wdn_net_info->ba_enable_flag[tid]  = wf_false;
                }
            }
            wdn_net_info->htpriv.agg_enable_bitmap = 0;
            wdn_net_info->htpriv.candidate_tid_bitmap = 0;
        }
    }

    return 0;
}
