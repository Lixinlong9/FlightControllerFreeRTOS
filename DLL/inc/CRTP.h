#ifndef _CRTP_H_
#define _CRTP_H_
#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "CRTP_Type.h"

#define CRTP_HEADER(port, channel) (((port & 0x0F) << 4) | (channel & 0x0F))
#define CRTP_IS_NULL_PACKET(P) ((P.header&0xF3)==0xF3)


#define CRTP_IS_NULL_PACKET(P) ((P.header&0xF3)==0xF3)

//add by shiqingziyang 2014.04.02
//#ifdef KEIL_PROJECT
//#pragma anon_unions
//#endif

typedef void (*CrtpCallback)(CRTPPacket *);
/**
 * Initialize the CRTP stack
 */
void crtpInit(void);

bool crtpTest(void);

/**
 * Initializes the queue and dispatch of an task.
 *
 * @param[in] taskId The id of the CRTP task
 */
void crtpInitTaskQueue(CRTPPort taskId);

/**
 * Register a callback to be called for a particular port.
 *
 * @param[in] port Crtp port for which the callback is set
 * @param[in] cb Callback that will be called when a packet is received on
 *            'port'.
 *
 * @note Only one callback can be registered per port! The last callback
 *       registered will be the one called
 */
void crtpRegisterPortCB(int port, CrtpCallback cb);

/**
 * Put a packet in the TX task
 *
 * If the TX stack is full, the oldest lowest priority packet is dropped
 *
 * @param[in] p CRTPPacket to send
 */
int crtpSendPacket(CRTPPacket *p);

/**
 * Put a packet in the TX task
 *
 * If the TX stack is full, the function block until one place is free (Good for console implementation)
 */
int crtpSendPacketBlock(CRTPPacket *p);

/**
 * Fetch a packet with a specidied task ID.
 *
 * @param[in]  taskId The id of the CRTP task
 * @param[out] p      The CRTP Packet with infomation (unchanged if nothing to fetch)
 *
 * @returns status of fetch from queue
 */
int crtpReceivePacket(CRTPPort taskId, CRTPPacket *p);

/**
 * Fetch a packet with a specidied task ID. Wait some time befor giving up
 *
 * @param[in]  taskId The id of the CRTP task
 * @param[out] p      The CRTP Packet with infomation (unchanged if nothing to fetch)
 * @param[in] wait    Wait time in milisecond
 *
 * @returns status of fetch from queue
 */
int crtpReceivePacketWait(CRTPPort taskId, CRTPPacket *p, int wait);

/**
 * Wait for a packet to arrive for the specified taskID
 *
 * @param[in]  taskId The id of the CRTP task
 * @paran[out] p      The CRTP Packet with information
 *
 * @return status of fetch from queue
 */
int crtpReceivePacketBlock(CRTPPort taskId, CRTPPacket *p);

void crtpPacketReveived(CRTPPacket *p);

/**
 * Function pointer structure to be filled by the CRTP link to permits CRTP to
 * use manu link
 */
struct crtpLinkOperations
{
  int (*setEnable)(bool enable);
  int (*sendPacket)(CRTPPacket *pk);
  int (*receivePacket)(CRTPPacket *pk);
  bool (*isConnected)(void);
  void (*reset)(void);
};

void crtpSetLink(struct crtpLinkOperations * lk);

/**
 * Check if the connection timeout has been reached, otherwise
 * we will assume that we are connected.
 *
 * @return true if conencted, otherwise false
 */
bool crtpIsConnected(void);

/**
 * Reset the CRTP communication by flushing all the queues that
 * contain packages.
 *
 * @return 0 for success
 */
int crtpReset(void);

#ifdef __cplusplus
}
#endif
#endif
