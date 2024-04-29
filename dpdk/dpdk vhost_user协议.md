---
title: vhost_user协议
date: 2023-12-21 14:53:25
categories:
- [dpdk,网络开发]
tags:
- dpdk
- 网络开发
- virtio_net
- vhost_user
- vdpa
---

# dpdk vhost_user 协议实现
|  vhost_user_protocol       | func | desc|
| :---        |    :----   |          :--- |
| VHOST_USER_NONE  | NULL  | - |
| VHOST_USER_GET_FEATURES  | vhost_user_get_features  | - |
| VHOST_USER_SET_FEATURES  | vhost_user_set_features  | - |
| VHOST_USER_SET_OWNER  | vhost_user_set_owner  | - |
| VHOST_USER_RESET_OWNER  | vhost_user_reset_owner  | - |
| VHOST_USER_SET_MEM_TABLE  | vhost_user_set_mem_table  | - |
| VHOST_USER_SET_LOG_BASE  | vhost_user_set_log_base  | - |
| VHOST_USER_SET_LOG_FD  | vhost_user_set_log_fd  | - |
| VHOST_USER_SET_VRING_NUM  | vhost_user_set_vring_num  | - |
| VHOST_USER_SET_VRING_ADDR  | vhost_user_set_vring_addr  | - |
| VHOST_USER_SET_VRING_BASE  | vhost_user_set_vring_base  | - |
| VHOST_USER_GET_VRING_BASE  | vhost_user_get_vring_base  | - |
| VHOST_USER_SET_VRING_KICK  | vhost_user_set_vring_kick  | - |
| VHOST_USER_SET_VRING_CALL  | vhost_user_set_vring_call  | - |
| VHOST_USER_SET_VRING_ERR  | vhost_user_set_vring_err  | - |
| VHOST_USER_GET_PROTOCOL_FEATURES  | vhost_user_get_protocol_features  | - |
| VHOST_USER_SET_PROTOCOL_FEATURES  | vhost_user_set_protocol_features  | - |
| VHOST_USER_GET_QUEUE_NUM  | vhost_user_get_queue_num  | - |
| VHOST_USER_SET_VRING_ENABLE  | vhost_user_set_vring_enable  | - |
| VHOST_USER_SEND_RARP  | vhost_user_send_rarp  | - |
| VHOST_USER_NET_SET_MTU  | vhost_user_net_set_mtu  | - |
| VHOST_USER_SET_SLAVE_REQ_FD  | vhost_user_set_req_fd  | - |
| VHOST_USER_IOTLB_MSG  | vhost_user_iotlb_msg  | - |
| VHOST_USER_POSTCOPY_ADVISE  | vhost_user_set_postcopy_advise  | - |
| VHOST_USER_POSTCOPY_LISTEN  | vhost_user_set_postcopy_listen  | - |
| VHOST_USER_POSTCOPY_END  | vhost_user_postcopy_end  | - |
| VHOST_USER_GET_INFLIGHT_FD  | vhost_user_get_inflight_fd  | - |
| VHOST_USER_SET_INFLIGHT_FD  | vhost_user_set_inflight_fd  | - |
| VHOST_USER_SET_STATUS  | vhost_user_set_status  | - |
| VHOST_USER_GET_STATUS  | vhost_user_get_status  | - |
| VHOST_USER_SET_CONFIG  | vhost_user_set_config  | - |
| VHOST_USER_GET_CONFIG  | vhost_user_get_config  | - |

# dpdk