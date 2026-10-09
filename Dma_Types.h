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
 * \note STM32L4 / STM32L4+ DMA1 / DMA2 have 7 channels. Peripheral request of the
 *       channel (\ref dma_PeriphReqId_t) is selected by DMA_CSELR register of the
 *       DMA (STM32L4 - fixed request mapping per channel) or routed by request
 *       multiplexer DMAMUX1 (STM32L4+ - any request on any channel). DMAMUX
 *       request generator and synchronization are not used by the module.
 *
 */

#ifndef DMA_DMA_TYPES_H
#define DMA_DMA_TYPES_H
/* ============================== INCLUDES ================================== */
#include "stdint.h"                         /* Module types definition        */
#include "Stm32_dma.h"                      /* DMA Registers definitions      */
#include "Stm32_dmamux.h"                   /* DMAMUX request identifications */
/* ========================== SYMBOLIC CONSTANTS ============================ */

/** Null pointer definition */
#define DMA_NULL_PTR                        ( ( void* ) 0u )

/** Maximal count of data transferred by one transfer (16 bit CNDTR register) */
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


/** Enumeration of available transfer channels of DMA peripheral */
typedef enum
{
    DMA_CHANNEL_1 = 0u, /**< DMA channel 1 */
    DMA_CHANNEL_2,      /**< DMA channel 2 */
    DMA_CHANNEL_3,      /**< DMA channel 3 */
    DMA_CHANNEL_4,      /**< DMA channel 4 */
    DMA_CHANNEL_5,      /**< DMA channel 5 */
    DMA_CHANNEL_6,      /**< DMA channel 6 */
#if defined(DMA1_Channel7)
    DMA_CHANNEL_7,      /**< DMA channel 7 */
#endif
#if defined(DMA1_Channel8)
    DMA_CHANNEL_8,      /**< DMA channel 8 */
#endif
    DMA_CHANNEL_CNT
}   dma_ChannelId_t;


/** DMA data transfer direction enumeration */
typedef enum
{
    DMA_DIR_PERIPH_TO_MEMORY = LL_DMA_DIRECTION_PERIPH_TO_MEMORY, /**< Peripheral to memory direction */
    DMA_DIR_MEMORY_TO_PERIPH = LL_DMA_DIRECTION_MEMORY_TO_PERIPH, /**< Memory to peripheral direction */
    DMA_DIR_MEMORY_TO_MEMORY = LL_DMA_DIRECTION_MEMORY_TO_MEMORY  /**< Memory to memory direction     */
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
 * \brief List of DMA request sources
 *
 * STM32L4+ (DMAMUX1): the request of the peripheral is routed to the DMA
 * channel by DMAMUX1 - any request can be served by any channel of both DMA
 * peripherals. Enumerator values are DMAMUX1 request identifications.
 *
 * STM32L4 (no DMAMUX): every channel selects one of up to 8 requests by its
 * DMA_CSELR field (RM0351 / RM0394 DMA request mapping) - the request can be
 * served only by the channels listed in the mapping table of the module
 * (\ref Dma_Set_PeripheralRequest returns error otherwise). Requests sharing one
 * request line of a channel (e.g. TIM1_CH4 / TIM1_TRIG / TIM1_COM) select the
 * same CSELR value. Enumerator values are module identifications only.
 *
 * Requests of peripherals not present on the selected MCU are excluded at
 * compile time. \ref DMA_REQ_MEM2MEM selects no peripheral request (memory to
 * memory transfer or software triggered transfer).
 */
typedef enum
{
#if defined(DMAMUX1)
    DMA_REQ_MEM2MEM            = LL_DMAMUX_REQ_MEM2MEM,         /**< Memory to memory transfer            */
    DMA_REQ_GENERATOR0         = LL_DMAMUX_REQ_GENERATOR0,      /**< DMAMUX request generator 0           */
    DMA_REQ_GENERATOR1         = LL_DMAMUX_REQ_GENERATOR1,      /**< DMAMUX request generator 1           */
    DMA_REQ_GENERATOR2         = LL_DMAMUX_REQ_GENERATOR2,      /**< DMAMUX request generator 2           */
    DMA_REQ_GENERATOR3         = LL_DMAMUX_REQ_GENERATOR3,      /**< DMAMUX request generator 3           */
    DMA_REQ_ADC1               = LL_DMAMUX_REQ_ADC1,            /**< ADC1 request                         */
#if defined(ADC2)
    DMA_REQ_ADC2               = LL_DMAMUX_REQ_ADC2,            /**< ADC2 request                         */
#endif /* ADC2 */
    DMA_REQ_DAC1_CH1           = LL_DMAMUX_REQ_DAC1_CH1,        /**< DAC1 channel 1 request               */
    DMA_REQ_DAC1_CH2           = LL_DMAMUX_REQ_DAC1_CH2,        /**< DAC1 channel 2 request               */
    DMA_REQ_TIM6_UP            = LL_DMAMUX_REQ_TIM6_UP,         /**< TIM6 update request                  */
    DMA_REQ_TIM7_UP            = LL_DMAMUX_REQ_TIM7_UP,         /**< TIM7 update request                  */
    DMA_REQ_SPI1_RX            = LL_DMAMUX_REQ_SPI1_RX,         /**< SPI1 RX request                      */
    DMA_REQ_SPI1_TX            = LL_DMAMUX_REQ_SPI1_TX,         /**< SPI1 TX request                      */
    DMA_REQ_SPI2_RX            = LL_DMAMUX_REQ_SPI2_RX,         /**< SPI2 RX request                      */
    DMA_REQ_SPI2_TX            = LL_DMAMUX_REQ_SPI2_TX,         /**< SPI2 TX request                      */
    DMA_REQ_SPI3_RX            = LL_DMAMUX_REQ_SPI3_RX,         /**< SPI3 RX request                      */
    DMA_REQ_SPI3_TX            = LL_DMAMUX_REQ_SPI3_TX,         /**< SPI3 TX request                      */
    DMA_REQ_I2C1_RX            = LL_DMAMUX_REQ_I2C1_RX,         /**< I2C1 RX request                      */
    DMA_REQ_I2C1_TX            = LL_DMAMUX_REQ_I2C1_TX,         /**< I2C1 TX request                      */
    DMA_REQ_I2C2_RX            = LL_DMAMUX_REQ_I2C2_RX,         /**< I2C2 RX request                      */
    DMA_REQ_I2C2_TX            = LL_DMAMUX_REQ_I2C2_TX,         /**< I2C2 TX request                      */
    DMA_REQ_I2C3_RX            = LL_DMAMUX_REQ_I2C3_RX,         /**< I2C3 RX request                      */
    DMA_REQ_I2C3_TX            = LL_DMAMUX_REQ_I2C3_TX,         /**< I2C3 TX request                      */
    DMA_REQ_I2C4_RX            = LL_DMAMUX_REQ_I2C4_RX,         /**< I2C4 RX request                      */
    DMA_REQ_I2C4_TX            = LL_DMAMUX_REQ_I2C4_TX,         /**< I2C4 TX request                      */
    DMA_REQ_USART1_RX          = LL_DMAMUX_REQ_USART1_RX,       /**< USART1 RX request                    */
    DMA_REQ_USART1_TX          = LL_DMAMUX_REQ_USART1_TX,       /**< USART1 TX request                    */
    DMA_REQ_USART2_RX          = LL_DMAMUX_REQ_USART2_RX,       /**< USART2 RX request                    */
    DMA_REQ_USART2_TX          = LL_DMAMUX_REQ_USART2_TX,       /**< USART2 TX request                    */
    DMA_REQ_USART3_RX          = LL_DMAMUX_REQ_USART3_RX,       /**< USART3 RX request                    */
    DMA_REQ_USART3_TX          = LL_DMAMUX_REQ_USART3_TX,       /**< USART3 TX request                    */
    DMA_REQ_UART4_RX           = LL_DMAMUX_REQ_UART4_RX,        /**< UART4 RX request                     */
    DMA_REQ_UART4_TX           = LL_DMAMUX_REQ_UART4_TX,        /**< UART4 TX request                     */
    DMA_REQ_UART5_RX           = LL_DMAMUX_REQ_UART5_RX,        /**< UART5 RX request                     */
    DMA_REQ_UART5_TX           = LL_DMAMUX_REQ_UART5_TX,        /**< UART5 TX request                     */
    DMA_REQ_LPUART1_RX         = LL_DMAMUX_REQ_LPUART1_RX,      /**< LPUART1 RX request                   */
    DMA_REQ_LPUART1_TX         = LL_DMAMUX_REQ_LPUART1_TX,      /**< LPUART1 TX request                   */
    DMA_REQ_SAI1_A             = LL_DMAMUX_REQ_SAI1_A,          /**< SAI1 block A request                 */
    DMA_REQ_SAI1_B             = LL_DMAMUX_REQ_SAI1_B,          /**< SAI1 block B request                 */
#if defined(SAI2)
    DMA_REQ_SAI2_A             = LL_DMAMUX_REQ_SAI2_A,          /**< SAI2 block A request                 */
    DMA_REQ_SAI2_B             = LL_DMAMUX_REQ_SAI2_B,          /**< SAI2 block B request                 */
#endif /* SAI2 */
    DMA_REQ_OSPI1              = LL_DMAMUX_REQ_OSPI1,           /**< OCTOSPI1 request                     */
#if defined(OCTOSPI2)
    DMA_REQ_OSPI2              = LL_DMAMUX_REQ_OSPI2,           /**< OCTOSPI2 request                     */
#endif /* OCTOSPI2 */
    DMA_REQ_TIM1_CH1           = LL_DMAMUX_REQ_TIM1_CH1,        /**< TIM1 channel 1 request               */
    DMA_REQ_TIM1_CH2           = LL_DMAMUX_REQ_TIM1_CH2,        /**< TIM1 channel 2 request               */
    DMA_REQ_TIM1_CH3           = LL_DMAMUX_REQ_TIM1_CH3,        /**< TIM1 channel 3 request               */
    DMA_REQ_TIM1_CH4           = LL_DMAMUX_REQ_TIM1_CH4,        /**< TIM1 channel 4 request               */
    DMA_REQ_TIM1_UP            = LL_DMAMUX_REQ_TIM1_UP,         /**< TIM1 update request                  */
    DMA_REQ_TIM1_TRIG          = LL_DMAMUX_REQ_TIM1_TRIG,       /**< TIM1 trigger request                 */
    DMA_REQ_TIM1_COM           = LL_DMAMUX_REQ_TIM1_COM,        /**< TIM1 commutation request             */
    DMA_REQ_TIM8_CH1           = LL_DMAMUX_REQ_TIM8_CH1,        /**< TIM8 channel 1 request               */
    DMA_REQ_TIM8_CH2           = LL_DMAMUX_REQ_TIM8_CH2,        /**< TIM8 channel 2 request               */
    DMA_REQ_TIM8_CH3           = LL_DMAMUX_REQ_TIM8_CH3,        /**< TIM8 channel 3 request               */
    DMA_REQ_TIM8_CH4           = LL_DMAMUX_REQ_TIM8_CH4,        /**< TIM8 channel 4 request               */
    DMA_REQ_TIM8_UP            = LL_DMAMUX_REQ_TIM8_UP,         /**< TIM8 update request                  */
    DMA_REQ_TIM8_TRIG          = LL_DMAMUX_REQ_TIM8_TRIG,       /**< TIM8 trigger request                 */
    DMA_REQ_TIM8_COM           = LL_DMAMUX_REQ_TIM8_COM,        /**< TIM8 commutation request             */
    DMA_REQ_TIM2_CH1           = LL_DMAMUX_REQ_TIM2_CH1,        /**< TIM2 channel 1 request               */
    DMA_REQ_TIM2_CH2           = LL_DMAMUX_REQ_TIM2_CH2,        /**< TIM2 channel 2 request               */
    DMA_REQ_TIM2_CH3           = LL_DMAMUX_REQ_TIM2_CH3,        /**< TIM2 channel 3 request               */
    DMA_REQ_TIM2_CH4           = LL_DMAMUX_REQ_TIM2_CH4,        /**< TIM2 channel 4 request               */
    DMA_REQ_TIM2_UP            = LL_DMAMUX_REQ_TIM2_UP,         /**< TIM2 update request                  */
    DMA_REQ_TIM3_CH1           = LL_DMAMUX_REQ_TIM3_CH1,        /**< TIM3 channel 1 request               */
    DMA_REQ_TIM3_CH2           = LL_DMAMUX_REQ_TIM3_CH2,        /**< TIM3 channel 2 request               */
    DMA_REQ_TIM3_CH3           = LL_DMAMUX_REQ_TIM3_CH3,        /**< TIM3 channel 3 request               */
    DMA_REQ_TIM3_CH4           = LL_DMAMUX_REQ_TIM3_CH4,        /**< TIM3 channel 4 request               */
    DMA_REQ_TIM3_UP            = LL_DMAMUX_REQ_TIM3_UP,         /**< TIM3 update request                  */
    DMA_REQ_TIM3_TRIG          = LL_DMAMUX_REQ_TIM3_TRIG,       /**< TIM3 trigger request                 */
    DMA_REQ_TIM4_CH1           = LL_DMAMUX_REQ_TIM4_CH1,        /**< TIM4 channel 1 request               */
    DMA_REQ_TIM4_CH2           = LL_DMAMUX_REQ_TIM4_CH2,        /**< TIM4 channel 2 request               */
    DMA_REQ_TIM4_CH3           = LL_DMAMUX_REQ_TIM4_CH3,        /**< TIM4 channel 3 request               */
    DMA_REQ_TIM4_CH4           = LL_DMAMUX_REQ_TIM4_CH4,        /**< TIM4 channel 4 request               */
    DMA_REQ_TIM4_UP            = LL_DMAMUX_REQ_TIM4_UP,         /**< TIM4 update request                  */
    DMA_REQ_TIM5_CH1           = LL_DMAMUX_REQ_TIM5_CH1,        /**< TIM5 channel 1 request               */
    DMA_REQ_TIM5_CH2           = LL_DMAMUX_REQ_TIM5_CH2,        /**< TIM5 channel 2 request               */
    DMA_REQ_TIM5_CH3           = LL_DMAMUX_REQ_TIM5_CH3,        /**< TIM5 channel 3 request               */
    DMA_REQ_TIM5_CH4           = LL_DMAMUX_REQ_TIM5_CH4,        /**< TIM5 channel 4 request               */
    DMA_REQ_TIM5_UP            = LL_DMAMUX_REQ_TIM5_UP,         /**< TIM5 update request                  */
    DMA_REQ_TIM5_TRIG          = LL_DMAMUX_REQ_TIM5_TRIG,       /**< TIM5 trigger request                 */
    DMA_REQ_TIM15_CH1          = LL_DMAMUX_REQ_TIM15_CH1,       /**< TIM15 channel 1 request              */
    DMA_REQ_TIM15_UP           = LL_DMAMUX_REQ_TIM15_UP,        /**< TIM15 update request                 */
    DMA_REQ_TIM15_TRIG         = LL_DMAMUX_REQ_TIM15_TRIG,      /**< TIM15 trigger request                */
    DMA_REQ_TIM15_COM          = LL_DMAMUX_REQ_TIM15_COM,       /**< TIM15 commutation request            */
    DMA_REQ_TIM16_CH1          = LL_DMAMUX_REQ_TIM16_CH1,       /**< TIM16 channel 1 request              */
    DMA_REQ_TIM16_UP           = LL_DMAMUX_REQ_TIM16_UP,        /**< TIM16 update request                 */
    DMA_REQ_TIM17_CH1          = LL_DMAMUX_REQ_TIM17_CH1,       /**< TIM17 channel 1 request              */
    DMA_REQ_TIM17_UP           = LL_DMAMUX_REQ_TIM17_UP,        /**< TIM17 update request                 */
    DMA_REQ_DFSDM1_FLT0        = LL_DMAMUX_REQ_DFSDM1_FLT0,     /**< DFSDM1 filter 0 request              */
    DMA_REQ_DFSDM1_FLT1        = LL_DMAMUX_REQ_DFSDM1_FLT1,     /**< DFSDM1 filter 1 request              */
    DMA_REQ_DFSDM1_FLT2        = LL_DMAMUX_REQ_DFSDM1_FLT2,     /**< DFSDM1 filter 2 request              */
    DMA_REQ_DFSDM1_FLT3        = LL_DMAMUX_REQ_DFSDM1_FLT3,     /**< DFSDM1 filter 3 request              */
#if defined(DCMI)
    DMA_REQ_DCMI               = LL_DMAMUX_REQ_DCMI,            /**< DCMI request                         */
#endif /* DCMI */
#if defined(AES)
    DMA_REQ_AES_IN             = LL_DMAMUX_REQ_AES_IN,          /**< AES data input request               */
    DMA_REQ_AES_OUT            = LL_DMAMUX_REQ_AES_OUT,         /**< AES data output request              */
#endif /* AES */
#if defined(HASH)
    DMA_REQ_HASH_IN            = LL_DMAMUX_REQ_HASH_IN,         /**< HASH data input request              */
#endif /* HASH */
#else
    DMA_REQ_MEM2MEM            = 0u,                            /**< Memory to memory transfer            */
    DMA_REQ_ADC1               ,                                /**< ADC1 request                         */
#if defined(ADC2)
    DMA_REQ_ADC2               ,                                /**< ADC2 request                         */
#endif /* ADC2 */
#if defined(ADC3)
    DMA_REQ_ADC3               ,                                /**< ADC3 request                         */
#endif /* ADC3 */
#if defined(DAC1)
    DMA_REQ_DAC1_CH1           ,                                /**< DAC1 channel 1 request               */
#endif /* DAC1 */
#if defined(DAC_CHANNEL2_SUPPORT)
    DMA_REQ_DAC1_CH2           ,                                /**< DAC1 channel 2 request               */
#endif /* DAC_CHANNEL2_SUPPORT */
#if defined(TIM6)
    DMA_REQ_TIM6_UP            ,                                /**< TIM6 update request                  */
#endif /* TIM6 */
#if defined(TIM7)
    DMA_REQ_TIM7_UP            ,                                /**< TIM7 update request                  */
#endif /* TIM7 */
    DMA_REQ_SPI1_RX            ,                                /**< SPI1 RX request                      */
    DMA_REQ_SPI1_TX            ,                                /**< SPI1 TX request                      */
#if defined(SPI2)
    DMA_REQ_SPI2_RX            ,                                /**< SPI2 RX request                      */
    DMA_REQ_SPI2_TX            ,                                /**< SPI2 TX request                      */
#endif /* SPI2 */
#if defined(SPI3)
    DMA_REQ_SPI3_RX            ,                                /**< SPI3 RX request                      */
    DMA_REQ_SPI3_TX            ,                                /**< SPI3 TX request                      */
#endif /* SPI3 */
    DMA_REQ_I2C1_RX            ,                                /**< I2C1 RX request                      */
    DMA_REQ_I2C1_TX            ,                                /**< I2C1 TX request                      */
#if defined(I2C2)
    DMA_REQ_I2C2_RX            ,                                /**< I2C2 RX request                      */
    DMA_REQ_I2C2_TX            ,                                /**< I2C2 TX request                      */
#endif /* I2C2 */
    DMA_REQ_I2C3_RX            ,                                /**< I2C3 RX request                      */
    DMA_REQ_I2C3_TX            ,                                /**< I2C3 TX request                      */
#if defined(I2C4)
    DMA_REQ_I2C4_RX            ,                                /**< I2C4 RX request                      */
    DMA_REQ_I2C4_TX            ,                                /**< I2C4 TX request                      */
#endif /* I2C4 */
    DMA_REQ_USART1_RX          ,                                /**< USART1 RX request                    */
    DMA_REQ_USART1_TX          ,                                /**< USART1 TX request                    */
    DMA_REQ_USART2_RX          ,                                /**< USART2 RX request                    */
    DMA_REQ_USART2_TX          ,                                /**< USART2 TX request                    */
#if defined(USART3)
    DMA_REQ_USART3_RX          ,                                /**< USART3 RX request                    */
    DMA_REQ_USART3_TX          ,                                /**< USART3 TX request                    */
#endif /* USART3 */
#if defined(UART4)
    DMA_REQ_UART4_RX           ,                                /**< UART4 RX request                     */
    DMA_REQ_UART4_TX           ,                                /**< UART4 TX request                     */
#endif /* UART4 */
#if defined(UART5)
    DMA_REQ_UART5_RX           ,                                /**< UART5 RX request                     */
    DMA_REQ_UART5_TX           ,                                /**< UART5 TX request                     */
#endif /* UART5 */
    DMA_REQ_LPUART1_RX         ,                                /**< LPUART1 RX request                   */
    DMA_REQ_LPUART1_TX         ,                                /**< LPUART1 TX request                   */
#if defined(SAI1)
    DMA_REQ_SAI1_A             ,                                /**< SAI1 block A request                 */
    DMA_REQ_SAI1_B             ,                                /**< SAI1 block B request                 */
#endif /* SAI1 */
#if defined(SAI2)
    DMA_REQ_SAI2_A             ,                                /**< SAI2 block A request                 */
    DMA_REQ_SAI2_B             ,                                /**< SAI2 block B request                 */
#endif /* SAI2 */
#if defined(QUADSPI)
    DMA_REQ_QSPI               ,                                /**< QUADSPI request                      */
#endif /* QUADSPI */
    DMA_REQ_TIM1_CH1           ,                                /**< TIM1 channel 1 request               */
    DMA_REQ_TIM1_CH2           ,                                /**< TIM1 channel 2 request               */
    DMA_REQ_TIM1_CH3           ,                                /**< TIM1 channel 3 request               */
    DMA_REQ_TIM1_CH4           ,                                /**< TIM1 channel 4 request               */
    DMA_REQ_TIM1_UP            ,                                /**< TIM1 update request                  */
    DMA_REQ_TIM1_TRIG          ,                                /**< TIM1 trigger request                 */
    DMA_REQ_TIM1_COM           ,                                /**< TIM1 commutation request             */
#if defined(TIM8)
    DMA_REQ_TIM8_CH1           ,                                /**< TIM8 channel 1 request               */
    DMA_REQ_TIM8_CH2           ,                                /**< TIM8 channel 2 request               */
    DMA_REQ_TIM8_CH3           ,                                /**< TIM8 channel 3 request               */
    DMA_REQ_TIM8_CH4           ,                                /**< TIM8 channel 4 request               */
    DMA_REQ_TIM8_UP            ,                                /**< TIM8 update request                  */
    DMA_REQ_TIM8_TRIG          ,                                /**< TIM8 trigger request                 */
    DMA_REQ_TIM8_COM           ,                                /**< TIM8 commutation request             */
#endif /* TIM8 */
    DMA_REQ_TIM2_CH1           ,                                /**< TIM2 channel 1 request               */
    DMA_REQ_TIM2_CH2           ,                                /**< TIM2 channel 2 request               */
    DMA_REQ_TIM2_CH3           ,                                /**< TIM2 channel 3 request               */
    DMA_REQ_TIM2_CH4           ,                                /**< TIM2 channel 4 request               */
    DMA_REQ_TIM2_UP            ,                                /**< TIM2 update request                  */
#if defined(TIM3)
    DMA_REQ_TIM3_CH1           ,                                /**< TIM3 channel 1 request               */
    DMA_REQ_TIM3_CH3           ,                                /**< TIM3 channel 3 request               */
    DMA_REQ_TIM3_CH4           ,                                /**< TIM3 channel 4 request               */
    DMA_REQ_TIM3_UP            ,                                /**< TIM3 update request                  */
    DMA_REQ_TIM3_TRIG          ,                                /**< TIM3 trigger request                 */
#endif /* TIM3 */
#if defined(TIM4)
    DMA_REQ_TIM4_CH1           ,                                /**< TIM4 channel 1 request               */
    DMA_REQ_TIM4_CH2           ,                                /**< TIM4 channel 2 request               */
    DMA_REQ_TIM4_CH3           ,                                /**< TIM4 channel 3 request               */
    DMA_REQ_TIM4_UP            ,                                /**< TIM4 update request                  */
#endif /* TIM4 */
#if defined(TIM5)
    DMA_REQ_TIM5_CH1           ,                                /**< TIM5 channel 1 request               */
    DMA_REQ_TIM5_CH2           ,                                /**< TIM5 channel 2 request               */
    DMA_REQ_TIM5_CH3           ,                                /**< TIM5 channel 3 request               */
    DMA_REQ_TIM5_CH4           ,                                /**< TIM5 channel 4 request               */
    DMA_REQ_TIM5_UP            ,                                /**< TIM5 update request                  */
    DMA_REQ_TIM5_TRIG          ,                                /**< TIM5 trigger request                 */
    DMA_REQ_TIM5_COM           ,                                /**< TIM5 commutation request             */
#endif /* TIM5 */
    DMA_REQ_TIM15_CH1          ,                                /**< TIM15 channel 1 request              */
    DMA_REQ_TIM15_UP           ,                                /**< TIM15 update request                 */
    DMA_REQ_TIM15_TRIG         ,                                /**< TIM15 trigger request                */
    DMA_REQ_TIM15_COM          ,                                /**< TIM15 commutation request            */
    DMA_REQ_TIM16_CH1          ,                                /**< TIM16 channel 1 request              */
    DMA_REQ_TIM16_UP           ,                                /**< TIM16 update request                 */
#if defined(TIM17)
    DMA_REQ_TIM17_CH1          ,                                /**< TIM17 channel 1 request              */
    DMA_REQ_TIM17_UP           ,                                /**< TIM17 update request                 */
#endif /* TIM17 */
#if defined(DFSDM1_Channel0)
    DMA_REQ_DFSDM1_FLT0        ,                                /**< DFSDM1 filter 0 request              */
    DMA_REQ_DFSDM1_FLT1        ,                                /**< DFSDM1 filter 1 request              */
#endif /* DFSDM1_Channel0 */
#if defined(DFSDM1_Filter2)
    DMA_REQ_DFSDM1_FLT2        ,                                /**< DFSDM1 filter 2 request              */
    DMA_REQ_DFSDM1_FLT3        ,                                /**< DFSDM1 filter 3 request              */
#endif /* DFSDM1_Filter2 */
#if defined(SWPMI1)
    DMA_REQ_SWPMI1_RX          ,                                /**< SWPMI1 RX request                    */
    DMA_REQ_SWPMI1_TX          ,                                /**< SWPMI1 TX request                    */
#endif /* SWPMI1 */
#if defined(SDMMC1)
    DMA_REQ_SDMMC1             ,                                /**< SDMMC1 request                       */
#endif /* SDMMC1 */
#if defined(DCMI)
    DMA_REQ_DCMI               ,                                /**< DCMI request                         */
#endif /* DCMI */
#if defined(AES)
    DMA_REQ_AES_IN             ,                                /**< AES data input request               */
    DMA_REQ_AES_OUT            ,                                /**< AES data output request              */
#endif /* AES */
#if defined(HASH)
    DMA_REQ_HASH_IN            ,                                /**< HASH data input request              */
#endif /* HASH */
    DMA_REQ_CNT                                                 /**< Count of request identifications     */
#endif /* DMAMUX1 */
}   dma_PeriphReqId_t;


/** DMA peripheral configuration structure */
typedef struct
{
    dma_PeriphId_t      DmaPeriphId;               /**< Specifies DMA number. Can be either DMA1 or DMA2
                                                        This parameter can be a value of \ref dma_PeriphId_t */

    dma_ChannelId_t     DmaChannel;                /**< Specifies configured DMA channel.
                                                        This parameter can be a value of \ref dma_ChannelId_t */

    dma_Direction_t     Direction;                 /**< Specifies if the data will be transferred from memory to peripheral,
                                                        from memory to memory or from peripheral to memory.
                                                        This parameter can be a value of \ref dma_Direction_t */

    dma_TransferMode_t  TransferMode;              /**< Specifies the normal or circular operation mode.
                                                        This parameter can be a value of \ref dma_TransferMode_t
                                                        @note: The circular mode cannot be used with the memory to memory
                                                        transfer direction (the transfer is refused by \ref Dma_Set_TransferActive) */

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
                                                        @note: Size different from PeriphTransferSize is handled by the channel
                                                        (data packing / unpacking - truncation or zero extension, RM0351 / RM0432) */

    dma_DataCount_t     DataCount;                 /**< Specifies the number of data to transfer, in data unit.
                                                        The data unit is equal to the peripheral (source) data size.
                                                        This parameter must be a value between Min_Data = 0 and Max_Data = 65535 */

    dma_PeriphReqId_t   PeripheralReqId;           /**< Specifies the peripheral request of the channel (CSELR / DMAMUX1).
                                                        This parameter can be a value of \ref dma_PeriphReqId_t */

    dma_Priority_t      Priority;                  /**< Specifies the channel priority level.
                                                        This parameter can be a value of \ref dma_Priority_t */

    dma_IsrCallback     TransferCompleteCallback;  /**< Pointer to user callback executed with transfer complete interrupt */

    dma_IsrCallback     HalfTransferCallback;      /**< Pointer to user callback executed with half transfer interrupt */

    dma_IsrCallback     TransferErrorCallback;     /**< Pointer to user callback executed with transfer error interrupt */

}   dma_ConfigStruct_t;

/* ========================== EXPORTED VARIABLES ============================ */

/* ========================= EXPORTED FUNCTIONS ============================= */


#endif /* DMA_DMA_TYPES_H */
