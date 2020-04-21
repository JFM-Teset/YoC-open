/**
 * File: mdm_netif_si.h
 * Brief: Public APIs of Sanechips
 *
 * Copyright (C) 2017 Sanechips Technology Co., Ltd.
 * Author: Zhao Hao
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#ifndef _MDM_DEV_H
#define _MDM_DEV_H

#include <arpa/inet.h>
#include <k_api.h>
#include <k_timer.h>
#include <k_list.h>

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 *                           Include header files                              *
 ******************************************************************************/

/*******************************************************************************
 *                             Macro definitions                               *
 ******************************************************************************/

#define MDM_CMD_SEND  0
#define MDM_CMD_FREE  1

#define MDM_PACKET_RECV 0//ͳ����
#define MDM_PACKET_SEND 1

//UPģʽIP��ǰcid��modem����ͷԤ������
#define CID_RESERVE 1  //��cidԤ����1���ֽڳ���
#define IP_HEAD_RESERVE 15  ////ipͷǰԤ����15���ֽڳ���

//����ģʽ:CP,UP
#define MDM_CP_MOD 0x01
#define MDM_UP_MOD 0x02

#define DATA_VIA_CP     0
#define DATA_VIA_UP     1

#define LOCATION_CID 1

#define ADDR_RECORD_NUM 16  //��ֵ��С��˼�rebuf�Ĵ�С��أ��������ǽ��ô�ֵ��Ϊ�ͺ˼�ͨ���������ֵ


/*��ʼֵ���������κβ���*/
#define V4V6_INVALID   0x00
/*��V4��ص���Ϣ��Ч������PDP������Ϣ*/
#define V4_VALID       0x01
/*��V6��ص���Ϣ��Ч������PDP������Ϣ*/
#define V6_VALID       0x02
/*V4��V6��ص���Ϣ��Ч������PDP������Ϣ*/
#define V46_VALID      0x03

/*PDPû����.��ʼֵ*/
#define PDP_V4V6_INACTIVE   V4V6_INVALID
/*PDP V4Э�鼤��*/
#define PDP_V4_ACTIVE       V4_VALID
/*PDP V6Э�鼤��*/
#define PDP_V6_ACTIVE       V6_VALID

/* Represents the PDP activation initiated by the local application, eg MMS,VoLTE */
#define PDP_LOCAL     0
#define PDP_NORMAL    2

#ifdef CONFIG_MODEM_DATA_ADDRESS_RECORD
#define MDM_DATA_ADDR_RECORD(data)  mdm_data_addr_record(data)
#define MDM_DATA_ADDR_FIND(data)  mdm_data_addr_find(data)
#else
#define MDM_DATA_ADDR_RECORD(data)
#define MDM_DATA_ADDR_FIND(data)
#endif

#define MDM_PACKET_NUM_PRINT()  mdm_pakcet_num_print()

/*******************************************************************************
 *                             Type definitions                                *
 ******************************************************************************/
/* PDP activation information,when the argument is 0, no valid value is indicated */
struct pdp_ip_info {

    /* eg V4V6_INVALID.. */
    u16_t ip46flag;
    u32_t ip;
    u32_t gateway;
    u32_t pri_dns;
    u32_t sec_dns;
    struct in6_addr ipv6;
};

//PDP������Ϣ��ר����PS�����ڣ�����cid����ʶ��ͬ��PS������
struct pdp_active_info {
    unsigned char c_id;//PDPΨһ��ʶ
    unsigned char mod_flags;
    unsigned char pdp_type;//�μ�PDP_LOCAL�Ⱥ�ֵ��
    struct pdp_ip_info act_info;
};

//PDPȥ������Ϣ��ר����PS�����ڣ�����cid����ʶ��ͬ��PS������
struct pdp_deactive_info {
    int c_id;//PDPΨһ��ʶ
    unsigned char ip46flag;
};

struct  modem_msg {

    u32_t cmd: 1;
    u32_t mod: 1;
    u32_t offset: 6;
    u32_t padding: 8;
    u32_t len: 16;
    void *data;
};

struct send_data_node {
    klist_t send_list;
    struct modem_msg *msg;
};

/*******************************************************************************
 *                       Global variable declarations                          *
 ******************************************************************************/

/*******************************************************************************
 *                       Global function declarations                          *
 ******************************************************************************/

int mdm_modflag_set(unsigned char mod_flags);

void mdm_cp_recv(void *data, unsigned int len, unsigned int c_id);

int mdm_netif_pdp_act(struct pdp_active_info *actinfo);

int mdm_netif_pdp_deact(unsigned int c_id, unsigned char ip46flag);

void mdm_psm_ref(unsigned int cmd);

void mdm_psm_unref();

int mdm_at_init(void);

int mdm_api_init(void);
/*******************************************************************************
 *                      Inline function implementations                        *
 ******************************************************************************/

#ifdef __cplusplus
}
#endif

#endif  // #ifndef _MDM_DEV_H


