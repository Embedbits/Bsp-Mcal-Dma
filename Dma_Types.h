/**
 * \defgroup Dma Dma
 * \brief Dma module
 */

/**
 * \author Mr.Nobody
 * \file Dma_Types.h
 * \ingroup Dma
 * \brief Dma module global types definition
 *
 * This file contains the types definitions used across the module and are
 * available for other modules through Port file.
 *
 * \note STM32F4 DMA has no request multiplexer (DMAMUX). Every DMA peripheral
 *       has 8 streams, transfer "channel" of the module interface
 *       (\ref dma_ChannelId_t) represents DMA stream. Peripheral request
 *       (\ref dma_PeriphReqId_t) is selected by channel selection of the stream
 *       (CHSEL) according to request mapping table of the reference manual
 *       (e.g. RM0090 tables "DMA1 request mapping" and "DMA2 request mapping").
 *
 */

#ifndef DMA_DMA_TYPES_H
#define DMA_DMA_TYPES_H
/* ============================== INCLUDES ================================== */
#include "stdint.h"                         /* Module types definition        */
#include "Stm32_dma.h"                      /* DMA Registers definitions      */
/* ========================== SYMBOLIC CONSTANTS ============================ */

/** Null pointer definition */
#define DMA_NULL_PTR                        ( ( void* ) 0u )

/** Maximal count of data transferred by one transfer (16 bit NDTR register) */
#define DMA_DATA_COUNT_MAX                  ( 0xFFFFu )

/* ========================== EXPORTED MACROS =============================== */

/* ============================== TYPEDEFS ================================== */

/** \brief Type signaling major version of SW module */
typedef uint8_t dma_MajorVersion_t;


/** \brief Type signaling minor version of SW module */
typedef uint8_t dma_MinorVersion_t;


/** \brief Type signaling patch version of SW module */
typedef uint8_t dma_PatchVersion_t;


/** \brief Type signaling actual version of SW module */
typedef struct
{
    dma_MajorVersion_t Major; /**< Major version */
    dma_MinorVersion_t Minor; /**< Minor version */
    dma_PatchVersion_t Patch; /**< Patch version */
}   dma_ModuleVersion_t;


/** Function status enumeration */
typedef enum
{
    DMA_FUNCTION_INACTIVE = 0u, /**< Function status is inactive */
    DMA_FUNCTION_ACTIVE         /**< Function status is active   */
}   dma_FunctionState_t;


/** Enumeration used to signal request processing state */
typedef enum
{
    DMA_REQUEST_ERROR = 0u, /**< Processing request failed  */
    DMA_REQUEST_OK          /**< Processing request succeed */
}   dma_RequestState_t;


/** Flag states enumeration */
typedef enum
{
    DMA_FLAG_INACTIVE = 0u, /**< Inactive flag state */
    DMA_FLAG_ACTIVE         /**< Active flag state   */
}   dma_FlagState_t;


typedef void ( *dma_IsrCallback )( void );


/** Format used for peripheral address */
typedef uint32_t dma_PeriphAddr_t;


/** Format used for memory address */
typedef uint32_t dma_MemoryAddr_t;


/** DMA peripherals enumeration list */
typedef enum
{
    DMA_PERIPH_1 = 0, /**< DMA peripheral 1 identification */
#if defined(DMA2)
    DMA_PERIPH_2,     /**< DMA peripheral 2 identification */
#endif
    DMA_PERIPH_CNT
}   dma_PeriphId_t;


/** Enumeration of available transfer channels (DMA streams) of DMA peripheral */
typedef enum
{
    DMA_STREAM_0 = 0u,  /**< DMA stream 0 */
    DMA_STREAM_1,       /**< DMA stream 1 */
    DMA_STREAM_2,       /**< DMA stream 2 */
    DMA_STREAM_3,       /**< DMA stream 3 */
    DMA_STREAM_4,       /**< DMA stream 4 */
    DMA_STREAM_5,       /**< DMA stream 5 */
    DMA_STREAM_6,       /**< DMA stream 6 */
    DMA_STREAM_7,       /**< DMA stream 7 */
    DMA_STREAM_CNT
}   dma_ChannelId_t;


/** DMA data transfer direction enumeration */
typedef enum
{
    DMA_DIR_PERIPH_TO_MEMORY = LL_DMA_DIRECTION_PERIPH_TO_MEMORY, /**< Peripheral to memory direction */
    DMA_DIR_MEMORY_TO_PERIPH = LL_DMA_DIRECTION_MEMORY_TO_PERIPH, /**< Memory to peripheral direction */
    DMA_DIR_MEMORY_TO_MEMORY = LL_DMA_DIRECTION_MEMORY_TO_MEMORY  /**< Memory to memory direction (DMA2 only) */
}   dma_Direction_t;


/** DMA data handling enumeration */
typedef enum
{
    DMA_TRANSFER_MODE_NORMAL   = LL_DMA_MODE_NORMAL,   /**< Normal transfer mode   */
    DMA_TRANSFER_MODE_CIRCULAR = LL_DMA_MODE_CIRCULAR, /**< Circular transfer mode */
}   dma_TransferMode_t;


/** Peripheral address increment options enumeration */
typedef enum
{
    DMA_PERIPH_ADDR_INCREMENT = LL_DMA_PERIPH_INCREMENT,  /**< Peripheral address will be incremented     */
    DMA_PERIPH_ADDR_STATIC    = LL_DMA_PERIPH_NOINCREMENT /**< Peripheral address will not be incremented */
}   dma_PeriphAddrInc_t;


/** Memory address increment options enumeration */
typedef enum
{
    DMA_MEMORY_ADDR_INCREMENT = LL_DMA_MEMORY_INCREMENT,  /**< Memory address will be incremented     */
    DMA_MEMORY_ADDR_STATIC    = LL_DMA_MEMORY_NOINCREMENT /**< Memory address will not be incremented */
}   dma_MemoryAddrInc_t;


/** Size of single transfer data block on memory side */
typedef enum
{
    DMA_TRANSFER_SIZE_8BIT = 0u, /**< Memory data transfer size : 8bits  */
    DMA_TRANSFER_SIZE_16BIT,     /**< Memory data transfer size : 16bits */
    DMA_TRANSFER_SIZE_32BIT,     /**< Memory data transfer size : 32bits */
    DMA_TRANSFER_SIZE_CNT        /**< Count of available options         */
}   dma_TransferSize_t;


/** Channel priority options enumeration */
typedef enum
{
    DMA_PRIORITY_LOW = 0u, /**< Priority level : Low       */
    DMA_PRIORITY_MEDIUM  , /**< Priority level : Medium    */
    DMA_PRIORITY_HIGH    , /**< Priority level : High      */
    DMA_PRIORITY_VERYHIGH, /**< Priority level : Very_High */
    DMA_PRIORITY_CNT       /**< Count of available options */
}   dma_Priority_t;


/** Representation of count of transfer through DMA */
typedef uint32_t dma_DataCount_t;


/**
 * \brief List of DMA request sources (channel selection of the stream)
 *
 * Peripheral request is connected to the stream through channel selection
 * (CHSEL). Combination of DMA peripheral, stream and channel of the requesting
 * peripheral is given by request mapping table of the reference manual, e.g.
 * USART2_RX is DMA1 stream 5 channel 4 on STM32F407.
 */
typedef enum
{
    DMA_REQ_CHANNEL_0    = LL_DMA_CHANNEL_0,  /**< Stream channel 0 selected  */
    DMA_REQ_CHANNEL_1    = LL_DMA_CHANNEL_1,  /**< Stream channel 1 selected  */
    DMA_REQ_CHANNEL_2    = LL_DMA_CHANNEL_2,  /**< Stream channel 2 selected  */
    DMA_REQ_CHANNEL_3    = LL_DMA_CHANNEL_3,  /**< Stream channel 3 selected  */
    DMA_REQ_CHANNEL_4    = LL_DMA_CHANNEL_4,  /**< Stream channel 4 selected  */
    DMA_REQ_CHANNEL_5    = LL_DMA_CHANNEL_5,  /**< Stream channel 5 selected  */
    DMA_REQ_CHANNEL_6    = LL_DMA_CHANNEL_6,  /**< Stream channel 6 selected  */
    DMA_REQ_CHANNEL_7    = LL_DMA_CHANNEL_7,  /**< Stream channel 7 selected  */
#if defined(DMA_SxCR_CHSEL_3)
    DMA_REQ_CHANNEL_8    = LL_DMA_CHANNEL_8,  /**< Stream channel 8 selected  */
    DMA_REQ_CHANNEL_9    = LL_DMA_CHANNEL_9,  /**< Stream channel 9 selected  */
    DMA_REQ_CHANNEL_10   = LL_DMA_CHANNEL_10, /**< Stream channel 10 selected */
    DMA_REQ_CHANNEL_11   = LL_DMA_CHANNEL_11, /**< Stream channel 11 selected */
    DMA_REQ_CHANNEL_12   = LL_DMA_CHANNEL_12, /**< Stream channel 12 selected */
    DMA_REQ_CHANNEL_13   = LL_DMA_CHANNEL_13, /**< Stream channel 13 selected */
    DMA_REQ_CHANNEL_14   = LL_DMA_CHANNEL_14, /**< Stream channel 14 selected */
    DMA_REQ_CHANNEL_15   = LL_DMA_CHANNEL_15, /**< Stream channel 15 selected */
#endif
    DMA_REQ_MEM2MEM      = LL_DMA_CHANNEL_0   /**< Memory to memory transfer, no peripheral request */
}   dma_PeriphReqId_t;


/** DMA peripheral configuration structure */
typedef struct
{
    dma_PeriphId_t      DmaPeriphId;               /**< Specifies DMA number. Can be either DMA1 or DMA2
                                                        This parameter can be a value of \ref dma_PeriphId_t */

    dma_ChannelId_t     DmaChannel;                /**< Specifies configured DMA stream.
                                                        This parameter can be a value of \ref dma_ChannelId_t */

    dma_Direction_t     Direction;                 /**< Specifies if the data will be transferred from memory to peripheral,
                                                        from memory to memory or from peripheral to memory.
                                                        This parameter can be a value of \ref dma_Direction_t
                                                        @note: Memory to memory direction is available on DMA2 only */

    dma_TransferMode_t  TransferMode;              /**< Specifies the normal or circular operation mode.
                                                        This parameter can be a value of \ref dma_TransferMode_t
                                                        @note: The circular buffer mode cannot be used if the memory to memory
                                                        data transfer direction is configured on the selected stream */

    dma_PeriphAddr_t    PeriphAddress;             /**< Specifies the peripheral base address for DMA transfer
                                                        or as Source base address in case of memory to memory transfer direction.
                                                        This parameter must be a value between Min_Data = 0 and Max_Data = 0xFFFFFFFF. */

    dma_MemoryAddr_t    MemoryAddress;             /**< Specifies the memory base address for DMA transfer
                                                        or as Destination base address in case of memory to memory transfer direction.
                                                        This parameter must be a value between Min_Data = 0 and Max_Data = 0xFFFFFFFF. */

    dma_PeriphAddrInc_t PeriphAddrIncrement;       /**< Specifies whether the Peripheral address or Source address in case of memory to memory transfer direction
                                                        is incremented or not.
                                                        This parameter can be a value of \ref dma_PeriphAddrInc_t */

    dma_MemoryAddrInc_t MemoryAddrIncrement;       /**< Specifies whether the Memory address or Destination address in case of memory to memory transfer direction
                                                        is incremented or not.
                                                        This parameter can be a value of \ref dma_MemoryAddrInc_t */

    dma_TransferSize_t  PeriphTransferSize;        /**< Specifies the Peripheral data size alignment or Source data size alignment (byte, half word, word)
                                                        in case of memory to memory transfer direction.
                                                        This parameter can be a value of \ref dma_TransferSize_t */

    dma_TransferSize_t  MemoryTransferSize;        /**< Specifies the Memory data size alignment or Destination data size alignment (byte, half word, word)
                                                        in case of memory to memory transfer direction.
                                                        This parameter can be a value of \ref dma_TransferSize_t
                                                        @note: Size different from PeriphTransferSize requires FIFO, which is
                                                        enabled by \ref Dma_Init in such case (direct mode otherwise) */

    dma_DataCount_t     DataCount;                 /**< Specifies the number of data to transfer, in data unit.
                                                        The data unit is equal to the peripheral (source) data size.
                                                        This parameter must be a value between Min_Data = 0 and Max_Data = 65535 */

    dma_PeriphReqId_t   PeripheralReqId;           /**< Specifies the peripheral request (channel selection of the stream).
                                                        This parameter can be a value of \ref dma_PeriphReqId_t */

    dma_Priority_t      Priority;                  /**< Specifies the stream priority level.
                                                        This parameter can be a value of \ref dma_Priority_t */

    dma_IsrCallback     TransferCompleteCallback;  /**< Pointer to user callback executed with transfer complete interrupt */

    dma_IsrCallback     HalfTransferCallback;      /**< Pointer to user callback executed with half transfer interrupt */

    dma_IsrCallback     TransferErrorCallback;     /**< Pointer to user callback executed with transfer error interrupt */

}   dma_ConfigStruct_t;

/* ========================== EXPORTED VARIABLES ============================ */

/* ========================= EXPORTED FUNCTIONS ============================= */


#endif /* DMA_DMA_TYPES_H */
