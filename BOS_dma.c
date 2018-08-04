/*
    BitzOS (BOS) V0.1.4 - Copyright (C) 2018 Hexabitz
    All rights reserved

    File Name     : BOS_dma.c
    Description   : Source code for BOS communication/backend DMAs. 
		
		Required MCU resources : 
		
			>> At least n UART RX DMA channels where n is number of module ports (up to number of available UARTs).
			>> At least one UART TX DMA channel.
			>> Any extra channels can be assigned to front-end.

*/
	
/* Includes ------------------------------------------------------------------*/
#include "BOS.h"
#include "BOS_DMA.h"


/* Exported variables ---------------------------------------------------------*/

uint8_t UARTRxBuf[NumOfPorts][MSG_RX_BUF_SIZE] = {0};
uint8_t UARTTxBuf[3][MSG_TX_BUF_SIZE] = {0};

UART_HandleTypeDef* dmaStreamDst[NumOfPorts] = {0};
uint32_t dmaStreamCount[NumOfPorts] = {0};
uint32_t dmaStreamTotal[NumOfPorts] = {0};
bool MsgDMAStopped[NumOfPorts] = {0};

/* Private variables ---------------------------------------------------------*/


/* External functions --------------------------------------------------------*/
extern void DMA_STREAM_Setup(UART_HandleTypeDef* huartSrc, UART_HandleTypeDef* huartDst, uint8_t num);


/* --- Stop a messaging DMA --- 
*/
void StopMsgDMA(uint8_t port)
{
	DMA_HandleTypeDef *hDMA;
	
	/* Select DMA struct */
	hDMA = &msgRxDMA[port-1];
	
	HAL_DMA_Abort(hDMA);
	hDMA->Instance->CNDTR = 0;
}

/*-----------------------------------------------------------*/

/* --- Stop a streaming DMA --- 
*/
void StopStreamDMA(uint8_t port)
{
	DMA_HandleTypeDef *hDMA;
	
	/* Select DMA struct */
	hDMA = &streamDMA[port-1];
	
	HAL_DMA_Abort(hDMA);
	hDMA->Instance->CNDTR = 0;
	dmaStreamCount[port-1] = 0;
	dmaStreamTotal[port-1] = 0;
	portStatus[GetPort(hDMA->Parent)] = FREE; 
	portStatus[GetPort(dmaStreamDst[port-1])] = FREE;	
	dmaStreamDst[port-1] = 0;
}

/*-----------------------------------------------------------*/

/* Switch messaging DMA channels to streaming 
*/
void SwitchMsgDMAToStream(uint8_t port)
{
	// TODO - Make sure all messages in the RX buffer have been parsed?
	
	// Stop the messaging DMA
	StopMsgDMA(port);
	
	// Initialize a streaming DMA using same channel
	DMA_STREAM_CH_Init(&streamDMA[port-1], msgRxDMA[port-1].Instance);
}

/*-----------------------------------------------------------*/

/* Switch streaming DMA channel to messaging 
*/
void SwitchStreamDMAToMsg(uint8_t port)
{
	// Stop the streaming DMA
	StopStreamDMA(port);
	
	// Initialize a messaging DMA using same channels
	DMA_MSG_RX_CH_Init(&msgRxDMA[port-1], streamDMA[port-1].Instance);	
	
	// Read this port again in messaging mode - TODO update to DMA read 
	HAL_UART_Receive_IT(GetUart(port), (uint8_t *)&cRxedChar, 1);		// TODO update
		
}

/*-----------------------------------------------------------*/

/* Setup and start a streaming DMA (port-to-port) 
*/
BOS_Status StartDMAstream(UART_HandleTypeDef* huartSrc, UART_HandleTypeDef* huartDst, uint8_t num)
{	
	uint8_t srcPort = GetPort(huartSrc);
	
	// 1. Check if single- or multi-cast 
	// 1.a. If single-cast, switch the DMA channel to streaming if it's available 
	if (portStatus[srcPort] == FREE)		// This port is not streaming so it's single-cast
	{
		SwitchMsgDMAToStream(srcPort);
	}
	// 1.b. If multi-cast, do some stuff - TODO
	else if (portStatus[srcPort] == STREAM)
	{
		return BOS_ERR_PORT_BUSY;		// Multi-casting not implemented right now
	}
	else
		return BOS_ERR_PORT_BUSY;

	// 2. Setup streaming destination
	dmaStreamDst[srcPort-1] = huartDst;
	
	// 3. Lock the ports 
	portStatus[srcPort] = STREAM;
	portStatus[GetPort(huartDst)] = STREAM;
	
	// 4. Initialize stream counter 
	dmaStreamCount[srcPort-1] = 0;
	
	// 5. Setup and start the DMA stream
	DMA_STREAM_Setup(huartSrc, huartDst, num);	
	
	return BOS_OK;
}

/*-----------------------------------------------------------*/

/* DMA interrupt service routine 
*/
void DMA_IRQHandler(uint8_t port)
{
	if (portStatus[port] != STREAM) {
		HAL_DMA_IRQHandler(&msgRxDMA[port-1]);
	} else {
		HAL_DMA_IRQHandler(&streamDMA[port-1]);
		if (dmaStreamTotal[port-1])
			++dmaStreamCount[port-1];
		if (dmaStreamCount[port-1] >= dmaStreamTotal[port-1]) {
			StopStreamDMA(port);
		}
	}
}

/************************ (C) COPYRIGHT HEXABITZ *****END OF FILE****/