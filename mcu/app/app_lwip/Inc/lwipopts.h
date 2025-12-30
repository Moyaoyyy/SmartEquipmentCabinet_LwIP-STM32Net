/**
 * @file    lwipopts.h
 * @author  MCD Application Team
 * @version V1.1.0
 * @date    31-July-2013
 * @brief   lwIP 选项配置文件。
 *          此文件基于 Utilities\lwip_v1.4.1\src\include\lwip\opt.h
 *
 */

#ifndef __LWIPOPTS_H__
#define __LWIPOPTS_H__

/**
 * SYS_LIGHTWEIGHT_PROT==1: 如果希望在缓冲区分配、释放和内存
 * 分配、释放期间为某些关键区域提供任务间保护。
 */
#define SYS_LIGHTWEIGHT_PROT 1

/**
 * NO_SYS==1: 提供非常基础的功能。否则，
 * 使用 lwIP 的完整功能。
 */
#define NO_SYS 0

/**
 * NO_SYS_NO_TIMERS==1: 当 NO_SYS==1 时放弃对 sys_timeout 的支持
 * 主要用于与旧版本的兼容性。
 */
#define NO_SYS_NO_TIMERS 0

/* ---------- 内存选项 ---------- */
/* MEM_ALIGNMENT: 应设置为编译 lwIP 的 CPU 的对齐方式。
   4 字节对齐 -> 将 MEM_ALIGNMENT 定义为 4,
   2 字节对齐 -> 将 MEM_ALIGNMENT 定义为 2。 */
#define MEM_ALIGNMENT 4

/* MEM_SIZE: 堆内存的大小。如果应用程序将发送
   大量需要复制的数据，则应将其设置得较高。 */
#define MEM_SIZE (15 * 1024)

/* MEMP_NUM_PBUF: memp 结构 pbuf 的数量。如果应用程序
   从 ROM (或其他静态内存) 发送大量数据，
   则应将其设置得较高。 */
#define MEMP_NUM_PBUF 25
/* MEMP_NUM_UDP_PCB: UDP 协议控制块的数量。
   每个活动的 UDP "连接" 一个。 */
#define MEMP_NUM_UDP_PCB 4
/* MEMP_NUM_TCP_PCB: 同时活动的 TCP
   连接的数量。 */
#define MEMP_NUM_TCP_PCB 6
/* MEMP_NUM_TCP_PCB_LISTEN: 监听的 TCP
   连接的数量。 */
#define MEMP_NUM_TCP_PCB_LISTEN 6
/* MEMP_NUM_TCP_SEG: 同时排队的 TCP
   段的数量。 */
#define MEMP_NUM_TCP_SEG 150
/* MEMP_NUM_SYS_TIMEOUT: 同时活动的
   超时的数量。 */
#define MEMP_NUM_SYS_TIMEOUT 6

/* ---------- Pbuf 选项 ---------- */
/* PBUF_POOL_SIZE: pbuf 池中的缓冲区数量。 */
#define PBUF_POOL_SIZE 45

/* PBUF_POOL_BUFSIZE: pbuf 池中每个 pbuf 的大小。 */
#define PBUF_POOL_BUFSIZE LWIP_MEM_ALIGN_SIZE(TCP_MSS + 40 + PBUF_LINK_HLEN)

/* ---------- TCP 选项 ---------- */
#define LWIP_TCP 1
#define TCP_TTL 255

/* 控制 TCP 是否应该将乱序到达的段
   排队。如果设备内存不足，则定义为 0。 */
#define TCP_QUEUE_OOSEQ 0

/* TCP 最大段大小。 */
#define TCP_MSS (1500 - 40) /* TCP_MSS = (以太网 MTU - IP 头大小 - TCP 头大小) */

/* TCP 发送缓冲区空间 (字节)。 */
#define TCP_SND_BUF (10 * TCP_MSS)

/*  TCP_SND_QUEUELEN: TCP 发送缓冲区空间 (pbuf 数量)。
    为使其正常工作，这至少必须是 (2 * TCP_SND_BUF/TCP_MSS)。 */

#define TCP_SND_QUEUELEN (8 * TCP_SND_BUF / TCP_MSS)

/* TCP 接收窗口。 */
#define TCP_WND (11 * TCP_MSS)

/* ---------- ICMP 选项 ---------- */
#define LWIP_ICMP 1

/* ---------- DHCP 选项 ---------- */
/* 如果希望接口的 DHCP 配置，
   请将 LWIP_DHCP 定义为 1。但是，lwIP 0.5.1 中未实现 DHCP，
   因此启用此选项目前不起作用。 */
#define LWIP_DHCP 1

/* ---------- UDP 选项 ---------- */
#define LWIP_UDP 1
#define UDP_TTL 255

/* ---------- 统计选项 ---------- */
#define LWIP_STATS 0
#define LWIP_PROVIDE_ERRNO 1

/* ---------- 链路回调选项 ---------- */
/* LWIP_NETIF_LINK_CALLBACK==1: 支持接口的回调函数，
 * 每当链路状态改变时(即链路断开)触发
 */
#define LWIP_NETIF_LINK_CALLBACK 1
/*
   --------------------------------------
   ---------- 校验和选项 ----------
   --------------------------------------
*/

/*
STM32F4x7 允许通过硬件计算和验证 IP、UDP、TCP 和 ICMP 校验和：
 - 要使用此功能，请保持以下定义不被注释。
 - 要禁用它并由 CPU 处理，请注释掉该定义。

注意：STM32 硬件校验和对 ICMP 支持有问题，会覆盖软件计算的校验和。
      因此我们只用硬件计算 IP 头校验和，其他由软件计算。
*/
#define CHECKSUM_BY_HARDWARE

#ifdef CHECKSUM_BY_HARDWARE
/* 只有 IP 头校验和由硬件生成，其他全部由软件生成 */
#define CHECKSUM_GEN_IP 0
#define CHECKSUM_GEN_UDP 1
#define CHECKSUM_GEN_TCP 1
#define CHECKSUM_GEN_ICMP 1
/* 接收校验和由硬件检查 */
#define CHECKSUM_CHECK_IP 0
#define CHECKSUM_CHECK_UDP 0
#define CHECKSUM_CHECK_TCP 0
#else
/* CHECKSUM_GEN_IP==1: 由软件为发出的 IP 数据包生成校验和。*/
#define CHECKSUM_GEN_IP 1
/* CHECKSUM_GEN_UDP==1: 由软件为发出的 UDP 数据包生成校验和。*/
#define CHECKSUM_GEN_UDP 1
/* CHECKSUM_GEN_TCP==1: 由软件为发出的 TCP 数据包生成校验和。*/
#define CHECKSUM_GEN_TCP 1
/* CHECKSUM_CHECK_IP==1: 由软件检查传入的 IP 数据包的校验和。*/
#define CHECKSUM_CHECK_IP 1
/* CHECKSUM_CHECK_UDP==1: 由软件检查传入的 UDP 数据包的校验和。*/
#define CHECKSUM_CHECK_UDP 1
/* CHECKSUM_CHECK_TCP==1: 由软件检查传入的 TCP 数据包的校验和。*/
#define CHECKSUM_CHECK_TCP 1
/* CHECKSUM_CHECK_ICMP==1: 由硬件检查传入的 ICMP 数据包的校验和。*/
#define CHECKSUM_GEN_ICMP 1
#endif

/*
   ----------------------------------------------
   ---------- 顺序层选项 ----------
   ----------------------------------------------
*/
/**
 * LWIP_NETCONN==1: 启用 Netconn API (需要使用 api_lib.c)
 */
#define LWIP_NETCONN 1

/*
   ------------------------------------
   ---------- Socket 选项 ----------
   ------------------------------------
*/
/**
 * LWIP_SOCKET==1: 启用 Socket API (需要使用 sockets.c)
 */
#define LWIP_SOCKET 0

/**
 * ---------------------------------------------------------------------------
 * Socket/Netconn 超时选项（强烈建议开启）
 * ---------------------------------------------------------------------------
 *
 * 说明：
 * - 你当前工程使用 Netconn API（LWIP_NETCONN=1）。
 * - 开启 LWIP_SO_RCVTIMEO / LWIP_SO_SNDTIMEO 后，netconn_recv/netconn_write
 *   才能设置“接收/发送超时”，避免在网络异常时永久阻塞某个任务。
 * - 这对 uplink HTTP 上报模块尤为重要。
 */
#define LWIP_SO_RCVTIMEO 1
#define LWIP_SO_SNDTIMEO 1

/*
   ---------------------------------
   ---------- OS 选项 ----------
   ---------------------------------
*/

#define DEFAULT_UDP_RECVMBOX_SIZE 10
#define DEFAULT_TCP_RECVMBOX_SIZE 10
#define DEFAULT_ACCEPTMBOX_SIZE 10
#define DEFAULT_THREAD_STACKSIZE 1024

#define TCPIP_THREAD_NAME "lwip"
#define TCPIP_THREAD_STACKSIZE 512
#define TCPIP_MBOX_SIZE 8
#define TCPIP_THREAD_PRIO 3

#endif /* __LWIPOPTS_H__ */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
