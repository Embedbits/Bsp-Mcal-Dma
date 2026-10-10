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
 * \note STM32H7 DMA1 / DMA2 have 8 streams each, transfer "channel" of the
 *       module interface (\ref dma_ChannelId_t) represents DMA stream. Peripheral
 *       request (\ref dma_PeriphReqId_t) is routed to the stream by the request
 *       multiplexer DMAMUX1 (DMAMUX1 channels 0 - 7 = DMA1 streams 0 - 7, channels
 *       8 - 15 = DMA2 streams 0 - 7) - any request can be served by any stream.
 * \note DMA1 / DMA2 can not access ITCM / DTCM - transfer buffers have to be
 *       placed in AXI SRAM (main RAM of the linker script) or D2 / D3 SRAM.
 *
 */

#ifndef DMA_DMA_TYPES_H
#define DMA_DMA_TYPES_H
/* ============================== INCLUDES ================================== */
#include "stdint.h"                         /* Module types definition        */
#include "Stm32_dma.h"                      /* DMA Registers definitions      */

#if defined(DMA1)
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
    DMA_DIR_MEMORY_TO_MEMORY = LL_DMA_DIRECTION_MEMORY_TO_MEMORY  /**< Memory to memory direction            */
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
 * \brief List of DMA request sources (DMAMUX1 request identification)
 *
 * Peripheral request is routed to the stream by DMAMUX1 - any request can be
 * served by any stream of DMA1 / DMA2. Requests of peripherals not available on
 * the selected device are not used (LL defines all requests of the line).
 */
typedef enum
{
#if defined(LL_DMAMUX1_REQ_MEM2MEM)
    DMA_REQ_MEM2MEM             = LL_DMAMUX1_REQ_MEM2MEM            , /**< Memory to memory transfer, no peripheral request */
#endif
#if defined(LL_DMAMUX1_REQ_GENERATOR0)
    DMA_REQ_GENERATOR0          = LL_DMAMUX1_REQ_GENERATOR0         , /**< DMAMUX1 request generator 0 */
#endif
#if defined(LL_DMAMUX1_REQ_GENERATOR1)
    DMA_REQ_GENERATOR1          = LL_DMAMUX1_REQ_GENERATOR1         , /**< DMAMUX1 request generator 1 */
#endif
#if defined(LL_DMAMUX1_REQ_GENERATOR2)
    DMA_REQ_GENERATOR2          = LL_DMAMUX1_REQ_GENERATOR2         , /**< DMAMUX1 request generator 2 */
#endif
#if defined(LL_DMAMUX1_REQ_GENERATOR3)
    DMA_REQ_GENERATOR3          = LL_DMAMUX1_REQ_GENERATOR3         , /**< DMAMUX1 request generator 3 */
#endif
#if defined(LL_DMAMUX1_REQ_GENERATOR4)
    DMA_REQ_GENERATOR4          = LL_DMAMUX1_REQ_GENERATOR4         , /**< DMAMUX1 request generator 4 */
#endif
#if defined(LL_DMAMUX1_REQ_GENERATOR5)
    DMA_REQ_GENERATOR5          = LL_DMAMUX1_REQ_GENERATOR5         , /**< DMAMUX1 request generator 5 */
#endif
#if defined(LL_DMAMUX1_REQ_GENERATOR6)
    DMA_REQ_GENERATOR6          = LL_DMAMUX1_REQ_GENERATOR6         , /**< DMAMUX1 request generator 6 */
#endif
#if defined(LL_DMAMUX1_REQ_GENERATOR7)
    DMA_REQ_GENERATOR7          = LL_DMAMUX1_REQ_GENERATOR7         , /**< DMAMUX1 request generator 7 */
#endif
#if defined(LL_DMAMUX1_REQ_ADC1)
    DMA_REQ_ADC1                = LL_DMAMUX1_REQ_ADC1               , /**< DMAMUX1 ADC1 request */
#endif
#if defined(LL_DMAMUX1_REQ_ADC2)
    DMA_REQ_ADC2                = LL_DMAMUX1_REQ_ADC2               , /**< DMAMUX1 ADC2 request */
#endif
#if defined(LL_DMAMUX1_REQ_TIM1_CH1)
    DMA_REQ_TIM1_CH1            = LL_DMAMUX1_REQ_TIM1_CH1           , /**< DMAMUX1 TIM1 CH1 request */
#endif
#if defined(LL_DMAMUX1_REQ_TIM1_CH2)
    DMA_REQ_TIM1_CH2            = LL_DMAMUX1_REQ_TIM1_CH2           , /**< DMAMUX1 TIM1 CH2 request */
#endif
#if defined(LL_DMAMUX1_REQ_TIM1_CH3)
    DMA_REQ_TIM1_CH3            = LL_DMAMUX1_REQ_TIM1_CH3           , /**< DMAMUX1 TIM1 CH3 request */
#endif
#if defined(LL_DMAMUX1_REQ_TIM1_CH4)
    DMA_REQ_TIM1_CH4            = LL_DMAMUX1_REQ_TIM1_CH4           , /**< DMAMUX1 TIM1 CH4 request */
#endif
#if defined(LL_DMAMUX1_REQ_TIM1_UP)
    DMA_REQ_TIM1_UP             = LL_DMAMUX1_REQ_TIM1_UP            , /**< DMAMUX1 TIM1 UP request */
#endif
#if defined(LL_DMAMUX1_REQ_TIM1_TRIG)
    DMA_REQ_TIM1_TRIG           = LL_DMAMUX1_REQ_TIM1_TRIG          , /**< DMAMUX1 TIM1 TRIG request */
#endif
#if defined(LL_DMAMUX1_REQ_TIM1_COM)
    DMA_REQ_TIM1_COM            = LL_DMAMUX1_REQ_TIM1_COM           , /**< DMAMUX1 TIM1 COM request */
#endif
#if defined(LL_DMAMUX1_REQ_TIM2_CH1)
    DMA_REQ_TIM2_CH1            = LL_DMAMUX1_REQ_TIM2_CH1           , /**< DMAMUX1 TIM2 CH1 request */
#endif
#if defined(LL_DMAMUX1_REQ_TIM2_CH2)
    DMA_REQ_TIM2_CH2            = LL_DMAMUX1_REQ_TIM2_CH2           , /**< DMAMUX1 TIM2 CH2 request */
#endif
#if defined(LL_DMAMUX1_REQ_TIM2_CH3)
    DMA_REQ_TIM2_CH3            = LL_DMAMUX1_REQ_TIM2_CH3           , /**< DMAMUX1 TIM2 CH3 request */
#endif
#if defined(LL_DMAMUX1_REQ_TIM2_CH4)
    DMA_REQ_TIM2_CH4            = LL_DMAMUX1_REQ_TIM2_CH4           , /**< DMAMUX1 TIM2 CH4 request */
#endif
#if defined(LL_DMAMUX1_REQ_TIM2_UP)
    DMA_REQ_TIM2_UP             = LL_DMAMUX1_REQ_TIM2_UP            , /**< DMAMUX1 TIM2 UP request */
#endif
#if defined(LL_DMAMUX1_REQ_TIM3_CH1)
    DMA_REQ_TIM3_CH1            = LL_DMAMUX1_REQ_TIM3_CH1           , /**< DMAMUX1 TIM3 CH1 request */
#endif
#if defined(LL_DMAMUX1_REQ_TIM3_CH2)
    DMA_REQ_TIM3_CH2            = LL_DMAMUX1_REQ_TIM3_CH2           , /**< DMAMUX1 TIM3 CH2 request */
#endif
#if defined(LL_DMAMUX1_REQ_TIM3_CH3)
    DMA_REQ_TIM3_CH3            = LL_DMAMUX1_REQ_TIM3_CH3           , /**< DMAMUX1 TIM3 CH3 request */
#endif
#if defined(LL_DMAMUX1_REQ_TIM3_CH4)
    DMA_REQ_TIM3_CH4            = LL_DMAMUX1_REQ_TIM3_CH4           , /**< DMAMUX1 TIM3 CH4 request */
#endif
#if defined(LL_DMAMUX1_REQ_TIM3_UP)
    DMA_REQ_TIM3_UP             = LL_DMAMUX1_REQ_TIM3_UP            , /**< DMAMUX1 TIM3 UP request */
#endif
#if defined(LL_DMAMUX1_REQ_TIM3_TRIG)
    DMA_REQ_TIM3_TRIG           = LL_DMAMUX1_REQ_TIM3_TRIG          , /**< DMAMUX1 TIM3 TRIG request */
#endif
#if defined(LL_DMAMUX1_REQ_TIM4_CH1)
    DMA_REQ_TIM4_CH1            = LL_DMAMUX1_REQ_TIM4_CH1           , /**< DMAMUX1 TIM4 CH1 request */
#endif
#if defined(LL_DMAMUX1_REQ_TIM4_CH2)
    DMA_REQ_TIM4_CH2            = LL_DMAMUX1_REQ_TIM4_CH2           , /**< DMAMUX1 TIM4 CH2 request */
#endif
#if defined(LL_DMAMUX1_REQ_TIM4_CH3)
    DMA_REQ_TIM4_CH3            = LL_DMAMUX1_REQ_TIM4_CH3           , /**< DMAMUX1 TIM4 CH3 request */
#endif
#if defined(LL_DMAMUX1_REQ_TIM4_UP)
    DMA_REQ_TIM4_UP             = LL_DMAMUX1_REQ_TIM4_UP            , /**< DMAMUX1 TIM4 UP request */
#endif
#if defined(LL_DMAMUX1_REQ_I2C1_RX)
    DMA_REQ_I2C1_RX             = LL_DMAMUX1_REQ_I2C1_RX            , /**< DMAMUX1 I2C1 RX request */
#endif
#if defined(LL_DMAMUX1_REQ_I2C1_TX)
    DMA_REQ_I2C1_TX             = LL_DMAMUX1_REQ_I2C1_TX            , /**< DMAMUX1 I2C1 TX request */
#endif
#if defined(LL_DMAMUX1_REQ_I2C2_RX)
    DMA_REQ_I2C2_RX             = LL_DMAMUX1_REQ_I2C2_RX            , /**< DMAMUX1 I2C2 RX request */
#endif
#if defined(LL_DMAMUX1_REQ_I2C2_TX)
    DMA_REQ_I2C2_TX             = LL_DMAMUX1_REQ_I2C2_TX            , /**< DMAMUX1 I2C2 TX request */
#endif
#if defined(LL_DMAMUX1_REQ_SPI1_RX)
    DMA_REQ_SPI1_RX             = LL_DMAMUX1_REQ_SPI1_RX            , /**< DMAMUX1 SPI1 RX request */
#endif
#if defined(LL_DMAMUX1_REQ_SPI1_TX)
    DMA_REQ_SPI1_TX             = LL_DMAMUX1_REQ_SPI1_TX            , /**< DMAMUX1 SPI1 TX request */
#endif
#if defined(LL_DMAMUX1_REQ_SPI2_RX)
    DMA_REQ_SPI2_RX             = LL_DMAMUX1_REQ_SPI2_RX            , /**< DMAMUX1 SPI2 RX request */
#endif
#if defined(LL_DMAMUX1_REQ_SPI2_TX)
    DMA_REQ_SPI2_TX             = LL_DMAMUX1_REQ_SPI2_TX            , /**< DMAMUX1 SPI2 TX request */
#endif
#if defined(LL_DMAMUX1_REQ_USART1_RX)
    DMA_REQ_USART1_RX           = LL_DMAMUX1_REQ_USART1_RX          , /**< DMAMUX1 USART1 RX request */
#endif
#if defined(LL_DMAMUX1_REQ_USART1_TX)
    DMA_REQ_USART1_TX           = LL_DMAMUX1_REQ_USART1_TX          , /**< DMAMUX1 USART1 TX request */
#endif
#if defined(LL_DMAMUX1_REQ_USART2_RX)
    DMA_REQ_USART2_RX           = LL_DMAMUX1_REQ_USART2_RX          , /**< DMAMUX1 USART2 RX request */
#endif
#if defined(LL_DMAMUX1_REQ_USART2_TX)
    DMA_REQ_USART2_TX           = LL_DMAMUX1_REQ_USART2_TX          , /**< DMAMUX1 USART2 TX request */
#endif
#if defined(LL_DMAMUX1_REQ_USART3_RX)
    DMA_REQ_USART3_RX           = LL_DMAMUX1_REQ_USART3_RX          , /**< DMAMUX1 USART3 RX request */
#endif
#if defined(LL_DMAMUX1_REQ_USART3_TX)
    DMA_REQ_USART3_TX           = LL_DMAMUX1_REQ_USART3_TX          , /**< DMAMUX1 USART3 TX request */
#endif
#if defined(LL_DMAMUX1_REQ_TIM8_CH1)
    DMA_REQ_TIM8_CH1            = LL_DMAMUX1_REQ_TIM8_CH1           , /**< DMAMUX1 TIM8 CH1 request */
#endif
#if defined(LL_DMAMUX1_REQ_TIM8_CH2)
    DMA_REQ_TIM8_CH2            = LL_DMAMUX1_REQ_TIM8_CH2           , /**< DMAMUX1 TIM8 CH2 request */
#endif
#if defined(LL_DMAMUX1_REQ_TIM8_CH3)
    DMA_REQ_TIM8_CH3            = LL_DMAMUX1_REQ_TIM8_CH3           , /**< DMAMUX1 TIM8 CH3 request */
#endif
#if defined(LL_DMAMUX1_REQ_TIM8_CH4)
    DMA_REQ_TIM8_CH4            = LL_DMAMUX1_REQ_TIM8_CH4           , /**< DMAMUX1 TIM8 CH4 request */
#endif
#if defined(LL_DMAMUX1_REQ_TIM8_UP)
    DMA_REQ_TIM8_UP             = LL_DMAMUX1_REQ_TIM8_UP            , /**< DMAMUX1 TIM8 UP request */
#endif
#if defined(LL_DMAMUX1_REQ_TIM8_TRIG)
    DMA_REQ_TIM8_TRIG           = LL_DMAMUX1_REQ_TIM8_TRIG          , /**< DMAMUX1 TIM8 TRIG request */
#endif
#if defined(LL_DMAMUX1_REQ_TIM8_COM)
    DMA_REQ_TIM8_COM            = LL_DMAMUX1_REQ_TIM8_COM           , /**< DMAMUX1 TIM8 COM request */
#endif
#if defined(LL_DMAMUX1_REQ_TIM5_CH1)
    DMA_REQ_TIM5_CH1            = LL_DMAMUX1_REQ_TIM5_CH1           , /**< DMAMUX1 TIM5 CH1 request */
#endif
#if defined(LL_DMAMUX1_REQ_TIM5_CH2)
    DMA_REQ_TIM5_CH2            = LL_DMAMUX1_REQ_TIM5_CH2           , /**< DMAMUX1 TIM5 CH2 request */
#endif
#if defined(LL_DMAMUX1_REQ_TIM5_CH3)
    DMA_REQ_TIM5_CH3            = LL_DMAMUX1_REQ_TIM5_CH3           , /**< DMAMUX1 TIM5 CH3 request */
#endif
#if defined(LL_DMAMUX1_REQ_TIM5_CH4)
    DMA_REQ_TIM5_CH4            = LL_DMAMUX1_REQ_TIM5_CH4           , /**< DMAMUX1 TIM5 CH4 request */
#endif
#if defined(LL_DMAMUX1_REQ_TIM5_UP)
    DMA_REQ_TIM5_UP             = LL_DMAMUX1_REQ_TIM5_UP            , /**< DMAMUX1 TIM5 UP request */
#endif
#if defined(LL_DMAMUX1_REQ_TIM5_TRIG)
    DMA_REQ_TIM5_TRIG           = LL_DMAMUX1_REQ_TIM5_TRIG          , /**< DMAMUX1 TIM5 TRIG request */
#endif
#if defined(LL_DMAMUX1_REQ_SPI3_RX)
    DMA_REQ_SPI3_RX             = LL_DMAMUX1_REQ_SPI3_RX            , /**< DMAMUX1 SPI3 RX request */
#endif
#if defined(LL_DMAMUX1_REQ_SPI3_TX)
    DMA_REQ_SPI3_TX             = LL_DMAMUX1_REQ_SPI3_TX            , /**< DMAMUX1 SPI3 TX request */
#endif
#if defined(LL_DMAMUX1_REQ_UART4_RX)
    DMA_REQ_UART4_RX            = LL_DMAMUX1_REQ_UART4_RX           , /**< DMAMUX1 UART4 RX request */
#endif
#if defined(LL_DMAMUX1_REQ_UART4_TX)
    DMA_REQ_UART4_TX            = LL_DMAMUX1_REQ_UART4_TX           , /**< DMAMUX1 UART4 TX request */
#endif
#if defined(LL_DMAMUX1_REQ_UART5_RX)
    DMA_REQ_UART5_RX            = LL_DMAMUX1_REQ_UART5_RX           , /**< DMAMUX1 UART5 RX request */
#endif
#if defined(LL_DMAMUX1_REQ_UART5_TX)
    DMA_REQ_UART5_TX            = LL_DMAMUX1_REQ_UART5_TX           , /**< DMAMUX1 UART5 TX request */
#endif
#if defined(LL_DMAMUX1_REQ_DAC1_CH1)
    DMA_REQ_DAC1_CH1            = LL_DMAMUX1_REQ_DAC1_CH1           , /**< DMAMUX1 DAC1 Channel 1 request */
#endif
#if defined(LL_DMAMUX1_REQ_DAC1_CH2)
    DMA_REQ_DAC1_CH2            = LL_DMAMUX1_REQ_DAC1_CH2           , /**< DMAMUX1 DAC1 Channel 2 request */
#endif
#if defined(LL_DMAMUX1_REQ_TIM6_UP)
    DMA_REQ_TIM6_UP             = LL_DMAMUX1_REQ_TIM6_UP            , /**< DMAMUX1 TIM6 UP request */
#endif
#if defined(LL_DMAMUX1_REQ_TIM7_UP)
    DMA_REQ_TIM7_UP             = LL_DMAMUX1_REQ_TIM7_UP            , /**< DMAMUX1 TIM7 UP request */
#endif
#if defined(LL_DMAMUX1_REQ_USART6_RX)
    DMA_REQ_USART6_RX           = LL_DMAMUX1_REQ_USART6_RX          , /**< DMAMUX1 USART6 RX request */
#endif
#if defined(LL_DMAMUX1_REQ_USART6_TX)
    DMA_REQ_USART6_TX           = LL_DMAMUX1_REQ_USART6_TX          , /**< DMAMUX1 USART6 TX request */
#endif
#if defined(LL_DMAMUX1_REQ_I2C3_RX)
    DMA_REQ_I2C3_RX             = LL_DMAMUX1_REQ_I2C3_RX            , /**< DMAMUX1 I2C3 RX request */
#endif
#if defined(LL_DMAMUX1_REQ_I2C3_TX)
    DMA_REQ_I2C3_TX             = LL_DMAMUX1_REQ_I2C3_TX            , /**< DMAMUX1 I2C3 TX request */
#endif
#if defined(LL_DMAMUX1_REQ_DCMI_PSSI)
    DMA_REQ_DCMI_PSSI           = LL_DMAMUX1_REQ_DCMI_PSSI          , /**< DMAMUX1 DCMI/PSSI request */
#endif
#if defined(LL_DMAMUX1_REQ_DCMI)
    DMA_REQ_DCMI                = LL_DMAMUX1_REQ_DCMI               , /**< DMAMUX1 DCMI request */
#endif
#if defined(LL_DMAMUX1_REQ_CRYP_IN)
    DMA_REQ_CRYP_IN             = LL_DMAMUX1_REQ_CRYP_IN            , /**< DMAMUX1 CRYP IN request */
#endif
#if defined(LL_DMAMUX1_REQ_CRYP_OUT)
    DMA_REQ_CRYP_OUT            = LL_DMAMUX1_REQ_CRYP_OUT           , /**< DMAMUX1 CRYP OUT request */
#endif
#if defined(LL_DMAMUX1_REQ_HASH_IN)
    DMA_REQ_HASH_IN             = LL_DMAMUX1_REQ_HASH_IN            , /**< DMAMUX1 HASH IN request */
#endif
#if defined(LL_DMAMUX1_REQ_UART7_RX)
    DMA_REQ_UART7_RX            = LL_DMAMUX1_REQ_UART7_RX           , /**< DMAMUX1 UART7 RX request */
#endif
#if defined(LL_DMAMUX1_REQ_UART7_TX)
    DMA_REQ_UART7_TX            = LL_DMAMUX1_REQ_UART7_TX           , /**< DMAMUX1 UART7 TX request */
#endif
#if defined(LL_DMAMUX1_REQ_UART8_RX)
    DMA_REQ_UART8_RX            = LL_DMAMUX1_REQ_UART8_RX           , /**< DMAMUX1 UART8 RX request */
#endif
#if defined(LL_DMAMUX1_REQ_UART8_TX)
    DMA_REQ_UART8_TX            = LL_DMAMUX1_REQ_UART8_TX           , /**< DMAMUX1 UART8 TX request */
#endif
#if defined(LL_DMAMUX1_REQ_SPI4_RX)
    DMA_REQ_SPI4_RX             = LL_DMAMUX1_REQ_SPI4_RX            , /**< DMAMUX1 SPI4 RX request */
#endif
#if defined(LL_DMAMUX1_REQ_SPI4_TX)
    DMA_REQ_SPI4_TX             = LL_DMAMUX1_REQ_SPI4_TX            , /**< DMAMUX1 SPI4 TX request */
#endif
#if defined(LL_DMAMUX1_REQ_SPI5_RX)
    DMA_REQ_SPI5_RX             = LL_DMAMUX1_REQ_SPI5_RX            , /**< DMAMUX1 SPI5 RX request */
#endif
#if defined(LL_DMAMUX1_REQ_SPI5_TX)
    DMA_REQ_SPI5_TX             = LL_DMAMUX1_REQ_SPI5_TX            , /**< DMAMUX1 SPI5 TX request */
#endif
#if defined(LL_DMAMUX1_REQ_SAI1_A)
    DMA_REQ_SAI1_A              = LL_DMAMUX1_REQ_SAI1_A             , /**< DMAMUX1 SAI1 A request */
#endif
#if defined(LL_DMAMUX1_REQ_SAI1_B)
    DMA_REQ_SAI1_B              = LL_DMAMUX1_REQ_SAI1_B             , /**< DMAMUX1 SAI1 B request */
#endif
#if defined(LL_DMAMUX1_REQ_SAI2_A)
    DMA_REQ_SAI2_A              = LL_DMAMUX1_REQ_SAI2_A             , /**< DMAMUX1 SAI2 A request */
#endif
#if defined(LL_DMAMUX1_REQ_SAI2_B)
    DMA_REQ_SAI2_B              = LL_DMAMUX1_REQ_SAI2_B             , /**< DMAMUX1 SAI2 B request */
#endif
#if defined(LL_DMAMUX1_REQ_SWPMI_RX)
    DMA_REQ_SWPMI_RX            = LL_DMAMUX1_REQ_SWPMI_RX           , /**< DMAMUX1 SWPMI RX request */
#endif
#if defined(LL_DMAMUX1_REQ_SWPMI_TX)
    DMA_REQ_SWPMI_TX            = LL_DMAMUX1_REQ_SWPMI_TX           , /**< DMAMUX1 SWPMI TX request */
#endif
#if defined(LL_DMAMUX1_REQ_SPDIF_RX_DT)
    DMA_REQ_SPDIF_RX_DT         = LL_DMAMUX1_REQ_SPDIF_RX_DT        , /**< DMAMUX1 SPDIF RXDT request */
#endif
#if defined(LL_DMAMUX1_REQ_SPDIF_RX_CS)
    DMA_REQ_SPDIF_RX_CS         = LL_DMAMUX1_REQ_SPDIF_RX_CS        , /**< DMAMUX1 SPDIF RXCS request */
#endif
#if defined(LL_DMAMUX1_REQ_HRTIM_MASTER)
    DMA_REQ_HRTIM_MASTER        = LL_DMAMUX1_REQ_HRTIM_MASTER       , /**< DMAMUX1 HRTIM1 Master request 1 */
#endif
#if defined(LL_DMAMUX1_REQ_HRTIM_TIMER_A)
    DMA_REQ_HRTIM_TIMER_A       = LL_DMAMUX1_REQ_HRTIM_TIMER_A      , /**< DMAMUX1 HRTIM1 Timer A request 2 */
#endif
#if defined(LL_DMAMUX1_REQ_HRTIM_TIMER_B)
    DMA_REQ_HRTIM_TIMER_B       = LL_DMAMUX1_REQ_HRTIM_TIMER_B      , /**< DMAMUX1 HRTIM1 Timer B request 3 */
#endif
#if defined(LL_DMAMUX1_REQ_HRTIM_TIMER_C)
    DMA_REQ_HRTIM_TIMER_C       = LL_DMAMUX1_REQ_HRTIM_TIMER_C      , /**< DMAMUX1 HRTIM1 Timer C request 4 */
#endif
#if defined(LL_DMAMUX1_REQ_HRTIM_TIMER_D)
    DMA_REQ_HRTIM_TIMER_D       = LL_DMAMUX1_REQ_HRTIM_TIMER_D      , /**< DMAMUX1 HRTIM1 Timer D request 5 */
#endif
#if defined(LL_DMAMUX1_REQ_HRTIM_TIMER_E)
    DMA_REQ_HRTIM_TIMER_E       = LL_DMAMUX1_REQ_HRTIM_TIMER_E      , /**< DMAMUX1 HRTIM1 Timer E request 6 */
#endif
#if defined(LL_DMAMUX1_REQ_DFSDM1_FLT0)
    DMA_REQ_DFSDM1_FLT0         = LL_DMAMUX1_REQ_DFSDM1_FLT0        , /**< DMAMUX1 DFSDM1 Filter0 request */
#endif
#if defined(LL_DMAMUX1_REQ_DFSDM1_FLT1)
    DMA_REQ_DFSDM1_FLT1         = LL_DMAMUX1_REQ_DFSDM1_FLT1        , /**< DMAMUX1 DFSDM1 Filter1 request */
#endif
#if defined(LL_DMAMUX1_REQ_DFSDM1_FLT2)
    DMA_REQ_DFSDM1_FLT2         = LL_DMAMUX1_REQ_DFSDM1_FLT2        , /**< DMAMUX1 DFSDM1 Filter2 request */
#endif
#if defined(LL_DMAMUX1_REQ_DFSDM1_FLT3)
    DMA_REQ_DFSDM1_FLT3         = LL_DMAMUX1_REQ_DFSDM1_FLT3        , /**< DMAMUX1 DFSDM1 Filter3 request */
#endif
#if defined(LL_DMAMUX1_REQ_TIM15_CH1)
    DMA_REQ_TIM15_CH1           = LL_DMAMUX1_REQ_TIM15_CH1          , /**< DMAMUX1 TIM15 CH1 request */
#endif
#if defined(LL_DMAMUX1_REQ_TIM15_UP)
    DMA_REQ_TIM15_UP            = LL_DMAMUX1_REQ_TIM15_UP           , /**< DMAMUX1 TIM15 UP request */
#endif
#if defined(LL_DMAMUX1_REQ_TIM15_TRIG)
    DMA_REQ_TIM15_TRIG          = LL_DMAMUX1_REQ_TIM15_TRIG         , /**< DMAMUX1 TIM15 TRIG request */
#endif
#if defined(LL_DMAMUX1_REQ_TIM15_COM)
    DMA_REQ_TIM15_COM           = LL_DMAMUX1_REQ_TIM15_COM          , /**< DMAMUX1 TIM15 COM request */
#endif
#if defined(LL_DMAMUX1_REQ_TIM16_CH1)
    DMA_REQ_TIM16_CH1           = LL_DMAMUX1_REQ_TIM16_CH1          , /**< DMAMUX1 TIM16 CH1 request */
#endif
#if defined(LL_DMAMUX1_REQ_TIM16_UP)
    DMA_REQ_TIM16_UP            = LL_DMAMUX1_REQ_TIM16_UP           , /**< DMAMUX1 TIM16 UP request */
#endif
#if defined(LL_DMAMUX1_REQ_TIM17_CH1)
    DMA_REQ_TIM17_CH1           = LL_DMAMUX1_REQ_TIM17_CH1          , /**< DMAMUX1 TIM17 CH1 request */
#endif
#if defined(LL_DMAMUX1_REQ_TIM17_UP)
    DMA_REQ_TIM17_UP            = LL_DMAMUX1_REQ_TIM17_UP           , /**< DMAMUX1 TIM17 UP request */
#endif
#if defined(LL_DMAMUX1_REQ_SAI3_A)
    DMA_REQ_SAI3_A              = LL_DMAMUX1_REQ_SAI3_A             , /**< DMAMUX1 SAI3 A request */
#endif
#if defined(LL_DMAMUX1_REQ_SAI3_B)
    DMA_REQ_SAI3_B              = LL_DMAMUX1_REQ_SAI3_B             , /**< DMAMUX1 SAI3 B request */
#endif
#if defined(LL_DMAMUX1_REQ_ADC3)
    DMA_REQ_ADC3                = LL_DMAMUX1_REQ_ADC3               , /**< DMAMUX1 ADC3  request */
#endif
#if defined(LL_DMAMUX1_REQ_UART9_RX)
    DMA_REQ_UART9_RX            = LL_DMAMUX1_REQ_UART9_RX           , /**< DMAMUX1 UART9 RX request */
#endif
#if defined(LL_DMAMUX1_REQ_UART9_TX)
    DMA_REQ_UART9_TX            = LL_DMAMUX1_REQ_UART9_TX           , /**< DMAMUX1 UART9 TX request */
#endif
#if defined(LL_DMAMUX1_REQ_USART10_RX)
    DMA_REQ_USART10_RX          = LL_DMAMUX1_REQ_USART10_RX         , /**< DMAMUX1 USART10 RX request */
#endif
#if defined(LL_DMAMUX1_REQ_USART10_TX)
    DMA_REQ_USART10_TX          = LL_DMAMUX1_REQ_USART10_TX         , /**< DMAMUX1 USART10 TX request */
#endif
#if defined(LL_DMAMUX1_REQ_FMAC_READ)
    DMA_REQ_FMAC_READ           = LL_DMAMUX1_REQ_FMAC_READ          , /**< DMAMUX1 FMAC Read request */
#endif
#if defined(LL_DMAMUX1_REQ_FMAC_WRITE)
    DMA_REQ_FMAC_WRITE          = LL_DMAMUX1_REQ_FMAC_WRITE         , /**< DMAMUX1 FMAC Write request */
#endif
#if defined(LL_DMAMUX1_REQ_CORDIC_READ)
    DMA_REQ_CORDIC_READ         = LL_DMAMUX1_REQ_CORDIC_READ        , /**< DMAMUX1 CORDIC Read request */
#endif
#if defined(LL_DMAMUX1_REQ_CORDIC_WRITE)
    DMA_REQ_CORDIC_WRITE        = LL_DMAMUX1_REQ_CORDIC_WRITE       , /**< DMAMUX1 CORDIC Write request */
#endif
#if defined(LL_DMAMUX1_REQ_I2C5_RX)
    DMA_REQ_I2C5_RX             = LL_DMAMUX1_REQ_I2C5_RX            , /**< DMAMUX1 I2C5 RX request */
#endif
#if defined(LL_DMAMUX1_REQ_I2C5_TX)
    DMA_REQ_I2C5_TX             = LL_DMAMUX1_REQ_I2C5_TX            , /**< DMAMUX1 I2C5 TX request */
#endif
#if defined(LL_DMAMUX1_REQ_TIM23_CH1)
    DMA_REQ_TIM23_CH1           = LL_DMAMUX1_REQ_TIM23_CH1          , /**< DMAMUX1 TIM23 CH1 request */
#endif
#if defined(LL_DMAMUX1_REQ_TIM23_CH2)
    DMA_REQ_TIM23_CH2           = LL_DMAMUX1_REQ_TIM23_CH2          , /**< DMAMUX1 TIM23 CH2 request */
#endif
#if defined(LL_DMAMUX1_REQ_TIM23_CH3)
    DMA_REQ_TIM23_CH3           = LL_DMAMUX1_REQ_TIM23_CH3          , /**< DMAMUX1 TIM23 CH3 request */
#endif
#if defined(LL_DMAMUX1_REQ_TIM23_CH4)
    DMA_REQ_TIM23_CH4           = LL_DMAMUX1_REQ_TIM23_CH4          , /**< DMAMUX1 TIM23 CH4 request */
#endif
#if defined(LL_DMAMUX1_REQ_TIM23_UP)
    DMA_REQ_TIM23_UP            = LL_DMAMUX1_REQ_TIM23_UP           , /**< DMAMUX1 TIM23 UP request */
#endif
#if defined(LL_DMAMUX1_REQ_TIM23_TRIG)
    DMA_REQ_TIM23_TRIG          = LL_DMAMUX1_REQ_TIM23_TRIG         , /**< DMAMUX1 TIM23 TRIG request */
#endif
#if defined(LL_DMAMUX1_REQ_TIM24_CH1)
    DMA_REQ_TIM24_CH1           = LL_DMAMUX1_REQ_TIM24_CH1          , /**< DMAMUX1 TIM24 CH1 request */
#endif
#if defined(LL_DMAMUX1_REQ_TIM24_CH2)
    DMA_REQ_TIM24_CH2           = LL_DMAMUX1_REQ_TIM24_CH2          , /**< DMAMUX1 TIM24 CH2 request */
#endif
#if defined(LL_DMAMUX1_REQ_TIM24_CH3)
    DMA_REQ_TIM24_CH3           = LL_DMAMUX1_REQ_TIM24_CH3          , /**< DMAMUX1 TIM24 CH3 request */
#endif
#if defined(LL_DMAMUX1_REQ_TIM24_CH4)
    DMA_REQ_TIM24_CH4           = LL_DMAMUX1_REQ_TIM24_CH4          , /**< DMAMUX1 TIM24 CH4 request */
#endif
#if defined(LL_DMAMUX1_REQ_TIM24_UP)
    DMA_REQ_TIM24_UP            = LL_DMAMUX1_REQ_TIM24_UP           , /**< DMAMUX1 TIM24 UP request */
#endif
#if defined(LL_DMAMUX1_REQ_TIM24_TRIG)
    DMA_REQ_TIM24_TRIG          = LL_DMAMUX1_REQ_TIM24_TRIG         , /**< DMAMUX1 TIM24 TRIG request */
#endif
#if defined(LL_DMAMUX1_REQ_GEN_DMAMUX1_CH0_EVT)
    DMA_REQ_GEN_DMAMUX1_CH0_EVT = LL_DMAMUX1_REQ_GEN_DMAMUX1_CH0_EVT, /**< DMAMUX1 Request generator Signal is DMAMUX1 Channel0 Event */
#endif
#if defined(LL_DMAMUX1_REQ_GEN_DMAMUX1_CH1_EVT)
    DMA_REQ_GEN_DMAMUX1_CH1_EVT = LL_DMAMUX1_REQ_GEN_DMAMUX1_CH1_EVT, /**< DMAMUX1 Request generator Signal is DMAMUX1 Channel1 Event */
#endif
#if defined(LL_DMAMUX1_REQ_GEN_DMAMUX1_CH2_EVT)
    DMA_REQ_GEN_DMAMUX1_CH2_EVT = LL_DMAMUX1_REQ_GEN_DMAMUX1_CH2_EVT, /**< DMAMUX1 Request generator Signal is DMAMUX1 Channel2 Event */
#endif
#if defined(LL_DMAMUX1_REQ_GEN_LPTIM1_OUT)
    DMA_REQ_GEN_LPTIM1_OUT      = LL_DMAMUX1_REQ_GEN_LPTIM1_OUT     , /**< DMAMUX1 Request generator Signal is LPTIM1 OUT */
#endif
#if defined(LL_DMAMUX1_REQ_GEN_LPTIM2_OUT)
    DMA_REQ_GEN_LPTIM2_OUT      = LL_DMAMUX1_REQ_GEN_LPTIM2_OUT     , /**< DMAMUX1 Request generator Signal is LPTIM2 OUT */
#endif
#if defined(LL_DMAMUX1_REQ_GEN_LPTIM3_OUT)
    DMA_REQ_GEN_LPTIM3_OUT      = LL_DMAMUX1_REQ_GEN_LPTIM3_OUT     , /**< DMAMUX1 Request generator Signal is LPTIM3 OUT */
#endif
#if defined(LL_DMAMUX1_REQ_GEN_EXTI0)
    DMA_REQ_GEN_EXTI0           = LL_DMAMUX1_REQ_GEN_EXTI0          , /**< DMAMUX1 Request generator Signal is EXTI0 IT */
#endif
#if defined(LL_DMAMUX1_REQ_GEN_TIM12_TRGO)
    DMA_REQ_GEN_TIM12_TRGO      = LL_DMAMUX1_REQ_GEN_TIM12_TRGO     , /**< DMAMUX1 Request generator Signal is TIM12 TRGO */
#endif
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
                                                        @note: Circular mode can not be used with memory to memory direction */

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

#endif /* DMA1 */

#endif /* DMA_DMA_TYPES_H */
