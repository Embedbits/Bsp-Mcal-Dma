/**
 * \author Mr.Nobody
 * \file Dma_Port.h
 * \ingroup Dma
 * \brief Dma module public functionality
 *
 * This file contains all available public functionality, any other files shall 
 * not used outside of the module.
 *
 */

#ifndef DMA_DMA_PORT_H
#define DMA_DMA_PORT_H

#ifdef __cplusplus
extern "C" {
#endif

/* ============================== INCLUDES ================================== */
#include "Dma_Types.h"                      /* Module types definition        */
/* ============================== TYPEDEFS ================================== */

/* ========================== SYMBOLIC CONSTANTS ============================ */

/* ========================== EXPORTED MACROS =============================== */

/* ========================== EXPORTED VARIABLES ============================ */

/* ========================= EXPORTED FUNCTIONS ============================= */

dma_ModuleVersion_t     Dma_Get_ModuleVersion               ( void );

dma_RequestState_t      Dma_Get_DefaultConfig               ( dma_ConfigStruct_t *clockConfig );
dma_RequestState_t      Dma_Init                            ( dma_ConfigStruct_t *clockConfig );

dma_RequestState_t      Dma_Set_TransferActive              ( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel );
dma_RequestState_t      Dma_Set_TransferInactive            ( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel );
dma_RequestState_t      Dma_Get_TransfertState              ( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_FunctionState_t *transferState );

dma_RequestState_t      Dma_Set_ChannelActive               ( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel );
dma_RequestState_t      Dma_Set_ChannelInactive             ( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel );
dma_RequestState_t      Dma_Get_ChannelState                ( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_FunctionState_t *channelState );

dma_RequestState_t      Dma_Set_Direction                   ( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_Direction_t direction );
dma_RequestState_t      Dma_Get_Direction                   ( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_Direction_t *direction );

dma_RequestState_t      Dma_Set_TransferMode                ( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_TransferMode_t transferMode );
dma_RequestState_t      Dma_Get_TransferMode                ( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_TransferMode_t *transferMode );

dma_RequestState_t      Dma_Set_PeriphAddr                  ( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_PeriphAddr_t periphAddr );
dma_RequestState_t      Dma_Get_PeriphAddr                  ( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_PeriphAddr_t *periphAddr );

dma_RequestState_t      Dma_Set_MemoryAddr                  ( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_MemoryAddr_t memoryAddr );
dma_RequestState_t      Dma_Get_MemoryAddr                  ( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_MemoryAddr_t *memoryAddr );

dma_RequestState_t      Dma_Set_PeriphAddrIncrement         ( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_PeriphAddrInc_t periphAddrInc );
dma_RequestState_t      Dma_Get_PeriphAddrIncrement         ( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_PeriphAddrInc_t *periphAddrInc );

dma_RequestState_t      Dma_Set_MemoryAddrIncrement         ( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_MemoryAddrInc_t memoryAddrInc );
dma_RequestState_t      Dma_Get_MemoryAddrIncrement         ( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_MemoryAddrInc_t *memoryAddrInc );

dma_RequestState_t      Dma_Set_PeriphTransferSize          ( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_PeriphTransferSize_t periphTransferSize);
dma_RequestState_t      Dma_Get_PeriphTransferSize          ( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_PeriphTransferSize_t *periphTransferSize );

dma_RequestState_t      Dma_Set_MemoryTransferSize          ( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_MemoryTransferSize_t memoryTransferSize );
dma_RequestState_t      Dma_Get_MemoryTransferSize          ( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_MemoryTransferSize_t *memoryTransferSize );

dma_RequestState_t      Dma_Set_DataCount                   ( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_DataCount_t dataCount );
dma_RequestState_t      Dma_Get_DataCount                   ( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_DataCount_t *dataCount );

dma_RequestState_t      Dma_Set_PeripheralRequest           ( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_PeriphReqId_t requestId );
dma_RequestState_t      Dma_Get_PeripheralRequest           ( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_PeriphReqId_t *requestId );

dma_RequestState_t      Dma_Set_Priority                    ( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_Priority_t dataCount );
dma_RequestState_t      Dma_Get_Priority                    ( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_Priority_t *dataCount );

/*------------------------- Interrupts functionality -------------------------*/

dma_RequestState_t      Dma_Set_InterruptActive             ( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel );
dma_RequestState_t      Dma_Set_InterruptInactive           ( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel );

dma_RequestState_t      Dma_Set_TransferCompleteIrqActive   ( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel );
dma_RequestState_t      Dma_Set_TransferCompleteIrqInactive ( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel );

dma_RequestState_t      Dma_Set_HalfTransferIrqActive       ( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel );
dma_RequestState_t      Dma_Set_HalfTransferIrqInactive     ( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel );

dma_RequestState_t      Dma_Set_TransferErrorIrqActive      ( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel );
dma_RequestState_t      Dma_Set_TransferErrorIrqInactive    ( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel );

dma_RequestState_t      Dma_Set_TransferCompleteIsrHandler  ( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_IsrCallback irqHandler );
dma_RequestState_t      Dma_Get_TransferCompleteIsrHandler  ( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_IsrCallback *irqHandler );

dma_RequestState_t      Dma_Set_HalfTransferIsrHandler      ( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_IsrCallback irqHandler );
dma_RequestState_t      Dma_Get_HalfTransferIsrHandler      ( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_IsrCallback *irqHandler );

dma_RequestState_t      Dma_Set_TransferErrorIsrHandler     ( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_IsrCallback irqHandler );
dma_RequestState_t      Dma_Get_TransferErrorIsrHandler     ( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_IsrCallback *irqHandler );

#ifdef __cplusplus
}
#endif

#endif /* DMA_DMA_PORT_H */

