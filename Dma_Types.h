/**
 * \author Mr.Nobody
 * \file Dma_Types.h
 * \ingroup Dma
 * \brief Dma module global types definition
 *
 * This file contains the types definitions used across the module and are 
 * available for other modules through Port file.
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


/** Frequency values type represented in Hz */
typedef uint32_t rcc_FreqHz_t;


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


/** Enumeration of available channels for all DMA peripherals */
typedef enum
{
    DMA_CHANNEL_1 = 0u, /**< DMA transfer channel 1 */
    DMA_CHANNEL_2,      /**< DMA transfer channel 2 */
    DMA_CHANNEL_3,      /**< DMA transfer channel 3 */
    DMA_CHANNEL_4,      /**< DMA transfer channel 4 */
    DMA_CHANNEL_5,      /**< DMA transfer channel 5 */
    DMA_CHANNEL_6,      /**< DMA transfer channel 6 */
    DMA_CHANNEL_7,      /**< DMA transfer channel 7 */
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


/** Size of single transfer data block on peripheral side */
typedef enum
{
    DMA_PERIPH_TRANSFER_SIZE_BYTE       = LL_DMA_PDATAALIGN_BYTE,     /**< Peripheral data transfer size : 8bits  */
    DMA_PERIPH_TRANSFER_SIZE_DOUBLEBYTE = LL_DMA_PDATAALIGN_HALFWORD, /**< Peripheral data transfer size : 16bits */
    DMA_PERIPH_TRANSFER_SIZE_QUADBYTE   = LL_DMA_PDATAALIGN_WORD      /**< Peripheral data transfer size : 32bits */
}   dma_PeriphTransferSize_t;


/** Size of single transfer data block on memory side */
typedef enum
{
    DMA_MEMORY_TRANSFER_SIZE_BYTE       = LL_DMA_MDATAALIGN_BYTE,     /**< Memory data transfer size : 8bits  */
    DMA_MEMORY_TRANSFER_SIZE_DOUBLEBYTE = LL_DMA_MDATAALIGN_HALFWORD, /**< Memory data transfer size : 16bits */
    DMA_MEMORY_TRANSFER_SIZE_QUADBYTE   = LL_DMA_MDATAALIGN_WORD      /**< Memory data transfer size : 32bits */
}   dma_MemoryTransferSize_t;


/** Channel priority options enumeration */
typedef enum
{
    DMA_PRIORITY_LOW      = LL_DMA_PRIORITY_LOW,     /**< Priority level : Low       */
    DMA_PRIORITY_MEDIUM   = LL_DMA_PRIORITY_MEDIUM,  /**< Priority level : Medium    */
    DMA_PRIORITY_HIGH     = LL_DMA_PRIORITY_HIGH,    /**< Priority level : High      */
    DMA_PRIORITY_VERYHIGH = LL_DMA_PRIORITY_VERYHIGH /**< Priority level : Very_High */
}   dma_Priority_t;


typedef enum
{
    DMA_MUX_CHANNEL_0  = LL_DMAMUX_CHANNEL_0  , /**< DMAMUX Channel 0 connected  to DMA1 Channel 1 */
    DMA_MUX_CHANNEL_1  = LL_DMAMUX_CHANNEL_1  , /**< DMAMUX Channel 1 connected  to DMA1 Channel 2 */
    DMA_MUX_CHANNEL_2  = LL_DMAMUX_CHANNEL_2  , /**< DMAMUX Channel 2 connected  to DMA1 Channel 3 */
    DMA_MUX_CHANNEL_3  = LL_DMAMUX_CHANNEL_3  , /**< DMAMUX Channel 3 connected  to DMA1 Channel 4 */
    DMA_MUX_CHANNEL_4  = LL_DMAMUX_CHANNEL_4  , /**< DMAMUX Channel 4 connected  to DMA1 Channel 5 */
    DMA_MUX_CHANNEL_5  = LL_DMAMUX_CHANNEL_5  , /**< DMAMUX Channel 5 connected  to DMA1 Channel 6 */
    DMA_MUX_CHANNEL_6  = LL_DMAMUX_CHANNEL_6  , /**< DMAMUX Channel 6 connected  to DMA1 Channel 7 */
    DMA_MUX_CHANNEL_7  = LL_DMAMUX_CHANNEL_7  , /**< DMAMUX Channel 7 connected  to DMA2 Channel 1 */
    DMA_MUX_CHANNEL_8  = LL_DMAMUX_CHANNEL_8  , /**< DMAMUX Channel 8 connected  to DMA2 Channel 2 */
    DMA_MUX_CHANNEL_9  = LL_DMAMUX_CHANNEL_9  , /**< DMAMUX Channel 9 connected  to DMA2 Channel 3 */
    DMA_MUX_CHANNEL_10 = LL_DMAMUX_CHANNEL_10 , /**< DMAMUX Channel 10 connected to DMA2 Channel 4 */
    DMA_MUX_CHANNEL_11 = LL_DMAMUX_CHANNEL_11 , /**< DMAMUX Channel 11 connected to DMA2 Channel 5 */
    DMA_MUX_CHANNEL_12 = LL_DMAMUX_CHANNEL_12 , /**< DMAMUX Channel 12 connected to DMA2 Channel 6 */
    DMA_MUX_CHANNEL_13 = LL_DMAMUX_CHANNEL_13 , /**< DMAMUX Channel 13 connected to DMA2 Channel 7 */
}   dma_MuxChannelId_t;


/** Representation of count of transfer through DMA */
typedef uint32_t dma_DataCount_t;


/** List of DMA request sources */
typedef enum
{
    DMA_REQ_MEM2MEM      = LL_DMAMUX_REQ_MEM2MEM,      /**< Memory to memory transfer  */
    DMA_REQ_GENERATOR0   = LL_DMAMUX_REQ_GENERATOR0,   /**< DMAMUX request generator 0 */
    DMA_REQ_GENERATOR1   = LL_DMAMUX_REQ_GENERATOR1,   /**< DMAMUX request generator 1 */
    DMA_REQ_GENERATOR2   = LL_DMAMUX_REQ_GENERATOR2,   /**< DMAMUX request generator 2 */
    DMA_REQ_GENERATOR3   = LL_DMAMUX_REQ_GENERATOR3,   /**< DMAMUX request generator 3 */
    DMA_REQ_ADC1         = LL_DMAMUX_REQ_ADC1,         /**< DMAMUX ADC1 request        */
    DMA_REQ_ADC2         = LL_DMAMUX_REQ_ADC2,         /**< DMAMUX ADC2 request        */
    DMA_REQ_ADC3         = LL_DMAMUX_REQ_ADC3,         /**< DMAMUX ADC3 request        */
    DMA_REQ_ADC4         = LL_DMAMUX_REQ_ADC4,         /**< DMAMUX ADC4 request        */
    DMA_REQ_ADC5         = LL_DMAMUX_REQ_ADC5,         /**< DMAMUX ADC5 request        */

    DMA_REQ_DAC1_CH1     = LL_DMAMUX_REQ_DAC1_CH1,     /**< DMAMUX DAC1 CH1 request    */
    DMA_REQ_DAC1_CH2     = LL_DMAMUX_REQ_DAC1_CH2,     /**< DMAMUX DAC1 CH2 request    */
    DMA_REQ_DAC2_CH1     = LL_DMAMUX_REQ_DAC2_CH1,     /**< DMAMUX DAC2 CH1 request    */
    DMA_REQ_DAC3_CH1     = LL_DMAMUX_REQ_DAC3_CH1,     /**< DMAMUX DAC3 CH1 request    */
    DMA_REQ_DAC3_CH2     = LL_DMAMUX_REQ_DAC3_CH2,     /**< DMAMUX DAC3 CH2 request    */
    DMA_REQ_DAC4_CH1     = LL_DMAMUX_REQ_DAC4_CH1,     /**< DMAMUX DAC4 CH1 request    */
    DMA_REQ_DAC4_CH2     = LL_DMAMUX_REQ_DAC4_CH2,     /**< DMAMUX DAC4 CH2 request    */

    DMA_REQ_TIM1_CH1     = LL_DMAMUX_REQ_TIM1_CH1,     /**< DMAMUX TIM1 CH1 request    */
    DMA_REQ_TIM1_CH2     = LL_DMAMUX_REQ_TIM1_CH2,     /**< DMAMUX TIM1 CH2 request    */
    DMA_REQ_TIM1_CH3     = LL_DMAMUX_REQ_TIM1_CH3,     /**< DMAMUX TIM1 CH3 request    */
    DMA_REQ_TIM1_CH4     = LL_DMAMUX_REQ_TIM1_CH4,     /**< DMAMUX TIM1 CH4 request    */
    DMA_REQ_TIM1_UP      = LL_DMAMUX_REQ_TIM1_UP,      /**< DMAMUX TIM1 UP request     */
    DMA_REQ_TIM1_TRIG    = LL_DMAMUX_REQ_TIM1_TRIG,    /**< DMAMUX TIM1 TRIG request   */
    DMA_REQ_TIM1_COM     = LL_DMAMUX_REQ_TIM1_COM,     /**< DMAMUX TIM1 COM request    */

    DMA_REQ_TIM2_CH1     = LL_DMAMUX_REQ_TIM2_CH1,     /**< DMAMUX TIM2 CH1 request    */
    DMA_REQ_TIM2_CH2     = LL_DMAMUX_REQ_TIM2_CH2,     /**< DMAMUX TIM2 CH2 request    */
    DMA_REQ_TIM2_CH3     = LL_DMAMUX_REQ_TIM2_CH3,     /**< DMAMUX TIM2 CH3 request    */
    DMA_REQ_TIM2_CH4     = LL_DMAMUX_REQ_TIM2_CH4,     /**< DMAMUX TIM2 CH4 request    */
    DMA_REQ_TIM2_UP      = LL_DMAMUX_REQ_TIM2_UP,      /**< DMAMUX TIM2 UP request     */

    DMA_REQ_TIM3_CH1     = LL_DMAMUX_REQ_TIM3_CH1,     /**< DMAMUX TIM3 CH1 request    */
    DMA_REQ_TIM3_CH2     = LL_DMAMUX_REQ_TIM3_CH2,     /**< DMAMUX TIM3 CH2 request    */
    DMA_REQ_TIM3_CH3     = LL_DMAMUX_REQ_TIM3_CH3,     /**< DMAMUX TIM3 CH3 request    */
    DMA_REQ_TIM3_CH4     = LL_DMAMUX_REQ_TIM3_CH4,     /**< DMAMUX TIM3 CH4 request    */
    DMA_REQ_TIM3_UP      = LL_DMAMUX_REQ_TIM3_UP,      /**< DMAMUX TIM3 UP request     */
    DMA_REQ_TIM3_TRIG    = LL_DMAMUX_REQ_TIM3_TRIG,    /**< DMAMUX TIM3 TRIG request   */

    DMA_REQ_TIM4_CH1     = LL_DMAMUX_REQ_TIM4_CH1,     /**< DMAMUX TIM4 CH1 request    */
    DMA_REQ_TIM4_CH2     = LL_DMAMUX_REQ_TIM4_CH2,     /**< DMAMUX TIM4 CH2 request    */
    DMA_REQ_TIM4_CH3     = LL_DMAMUX_REQ_TIM4_CH3,     /**< DMAMUX TIM4 CH3 request    */
    DMA_REQ_TIM4_CH4     = LL_DMAMUX_REQ_TIM4_CH4,     /**< DMAMUX TIM4 CH4 request    */
    DMA_REQ_TIM4_UP      = LL_DMAMUX_REQ_TIM4_UP,      /**< DMAMUX TIM4 UP request     */

    DMA_REQ_TIM5_CH1     = LL_DMAMUX_REQ_TIM5_CH1,     /**< DMAMUX TIM5 CH1 request    */
    DMA_REQ_TIM5_CH2     = LL_DMAMUX_REQ_TIM5_CH2,     /**< DMAMUX TIM5 CH2 request    */
    DMA_REQ_TIM5_CH3     = LL_DMAMUX_REQ_TIM5_CH3,     /**< DMAMUX TIM5 CH3 request    */
    DMA_REQ_TIM5_CH4     = LL_DMAMUX_REQ_TIM5_CH4,     /**< DMAMUX TIM5 CH4 request    */
    DMA_REQ_TIM5_UP      = LL_DMAMUX_REQ_TIM5_UP,      /**< DMAMUX TIM5 UP request     */
    DMA_REQ_TIM5_TRIG    = LL_DMAMUX_REQ_TIM5_TRIG,    /**< DMAMUX TIM5 TRIG request   */

    DMA_REQ_TIM6_UP      = LL_DMAMUX_REQ_TIM6_UP,      /**< DMAMUX TIM6 UP request     */

    DMA_REQ_TIM7_UP      = LL_DMAMUX_REQ_TIM7_UP,      /**< DMAMUX TIM7 UP request     */

    DMA_REQ_TIM8_CH1     = LL_DMAMUX_REQ_TIM8_CH1,     /**< DMAMUX TIM8 CH1 request    */
    DMA_REQ_TIM8_CH2     = LL_DMAMUX_REQ_TIM8_CH2,     /**< DMAMUX TIM8 CH2 request    */
    DMA_REQ_TIM8_CH3     = LL_DMAMUX_REQ_TIM8_CH3,     /**< DMAMUX TIM8 CH3 request    */
    DMA_REQ_TIM8_CH4     = LL_DMAMUX_REQ_TIM8_CH4,     /**< DMAMUX TIM8 CH4 request    */
    DMA_REQ_TIM8_UP      = LL_DMAMUX_REQ_TIM8_UP,      /**< DMAMUX TIM8 UP request     */
    DMA_REQ_TIM8_TRIG    = LL_DMAMUX_REQ_TIM8_TRIG,    /**< DMAMUX TIM8 TRIG request   */
    DMA_REQ_TIM8_COM     = LL_DMAMUX_REQ_TIM8_COM,     /**< DMAMUX TIM8 COM request    */

    DMA_REQ_TIM15_CH1    = LL_DMAMUX_REQ_TIM15_CH1,    /**< DMAMUX TIM15 CH1 request   */
    DMA_REQ_TIM15_UP     = LL_DMAMUX_REQ_TIM15_UP,     /**< DMAMUX TIM15 UP request    */
    DMA_REQ_TIM15_TRIG   = LL_DMAMUX_REQ_TIM15_TRIG,   /**< DMAMUX TIM15 TRIG request  */
    DMA_REQ_TIM15_COM    = LL_DMAMUX_REQ_TIM15_COM,    /**< DMAMUX TIM15 COM request   */

    DMA_REQ_TIM16_CH1    = LL_DMAMUX_REQ_TIM16_CH1,    /**< DMAMUX TIM16 CH1 request   */
    DMA_REQ_TIM16_UP     = LL_DMAMUX_REQ_TIM16_UP,     /**< DMAMUX TIM16 UP request    */

    DMA_REQ_TIM17_CH1    = LL_DMAMUX_REQ_TIM17_CH1,    /**< DMAMUX TIM17 CH1 request   */
    DMA_REQ_TIM17_UP     = LL_DMAMUX_REQ_TIM17_UP,     /**< DMAMUX TIM17 UP request    */

    DMA_REQ_TIM20_CH1    = LL_DMAMUX_REQ_TIM20_CH1,    /**< DMAMUX TIM20 CH1 request   */
    DMA_REQ_TIM20_CH2    = LL_DMAMUX_REQ_TIM20_CH2,    /**< DMAMUX TIM20 CH2 request   */
    DMA_REQ_TIM20_CH3    = LL_DMAMUX_REQ_TIM20_CH3,    /**< DMAMUX TIM20 CH3 request   */
    DMA_REQ_TIM20_CH4    = LL_DMAMUX_REQ_TIM20_CH4,    /**< DMAMUX TIM20 CH4 request   */
    DMA_REQ_TIM20_UP     = LL_DMAMUX_REQ_TIM20_UP,     /**< DMAMUX TIM20 UP request    */
    DMA_REQ_TIM20_TRIG   = LL_DMAMUX_REQ_TIM20_TRIG,   /**< DMAMUX TIM20 TRIG request  */
    DMA_REQ_TIM20_COM    = LL_DMAMUX_REQ_TIM20_COM,    /**< DMAMUX TIM20 COM request   */

    DMA_REQ_HRTIM_M      = LL_DMAMUX_REQ_HRTIM1_M,     /**< DMAMUX HRTIM M request     */
    DMA_REQ_HRTIM_A      = LL_DMAMUX_REQ_HRTIM1_A,     /**< DMAMUX HRTIM A request     */
    DMA_REQ_HRTIM_B      = LL_DMAMUX_REQ_HRTIM1_B,     /**< DMAMUX HRTIM B request     */
    DMA_REQ_HRTIM_C      = LL_DMAMUX_REQ_HRTIM1_C,     /**< DMAMUX HRTIM C request     */
    DMA_REQ_HRTIM_D      = LL_DMAMUX_REQ_HRTIM1_D,     /**< DMAMUX HRTIM D request     */
    DMA_REQ_HRTIM_E      = LL_DMAMUX_REQ_HRTIM1_E,     /**< DMAMUX HRTIM E request     */
    DMA_REQ_HRTIM_F      = LL_DMAMUX_REQ_HRTIM1_F,     /**< DMAMUX HRTIM F request     */

    DMA_REQ_SPI1_RX      = LL_DMAMUX_REQ_SPI1_RX,      /**< DMAMUX SPI1 RX request     */
    DMA_REQ_SPI1_TX      = LL_DMAMUX_REQ_SPI1_TX,      /**< DMAMUX SPI1 TX request     */
    DMA_REQ_SPI2_RX      = LL_DMAMUX_REQ_SPI2_RX,      /**< DMAMUX SPI2 RX request     */
    DMA_REQ_SPI2_TX      = LL_DMAMUX_REQ_SPI2_TX,      /**< DMAMUX SPI2 TX request     */
    DMA_REQ_SPI3_RX      = LL_DMAMUX_REQ_SPI3_RX,      /**< DMAMUX SPI3 RX request     */
    DMA_REQ_SPI3_TX      = LL_DMAMUX_REQ_SPI3_TX,      /**< DMAMUX SPI3 TX request     */
    DMA_REQ_SPI4_RX      = LL_DMAMUX_REQ_SPI4_RX,      /**< DMAMUX SPI4 RX request     */
    DMA_REQ_SPI4_TX      = LL_DMAMUX_REQ_SPI4_TX,      /**< DMAMUX SPI4 TX request     */

    DMA_REQ_I2C1_RX      = LL_DMAMUX_REQ_I2C1_RX,      /**< DMAMUX I2C1 RX request     */
    DMA_REQ_I2C1_TX      = LL_DMAMUX_REQ_I2C1_TX,      /**< DMAMUX I2C1 TX request     */
    DMA_REQ_I2C2_RX      = LL_DMAMUX_REQ_I2C2_RX,      /**< DMAMUX I2C2 RX request     */
    DMA_REQ_I2C2_TX      = LL_DMAMUX_REQ_I2C2_TX,      /**< DMAMUX I2C2 TX request     */
    DMA_REQ_I2C3_RX      = LL_DMAMUX_REQ_I2C3_RX,      /**< DMAMUX I2C3 RX request     */
    DMA_REQ_I2C3_TX      = LL_DMAMUX_REQ_I2C3_TX,      /**< DMAMUX I2C3 TX request     */
    DMA_REQ_I2C4_RX      = LL_DMAMUX_REQ_I2C4_RX,      /**< DMAMUX I2C4 RX request     */
    DMA_REQ_I2C4_TX      = LL_DMAMUX_REQ_I2C4_TX,      /**< DMAMUX I2C4 TX request     */

    DMA_REQ_USART1_RX    = LL_DMAMUX_REQ_USART1_RX,    /**< DMAMUX USART1 RX request   */
    DMA_REQ_USART1_TX    = LL_DMAMUX_REQ_USART1_TX,    /**< DMAMUX USART1 TX request   */
    DMA_REQ_USART2_RX    = LL_DMAMUX_REQ_USART2_RX,    /**< DMAMUX USART2 RX request   */
    DMA_REQ_USART2_TX    = LL_DMAMUX_REQ_USART2_TX,    /**< DMAMUX USART2 TX request   */
    DMA_REQ_USART3_RX    = LL_DMAMUX_REQ_USART3_RX,    /**< DMAMUX USART3 RX request   */
    DMA_REQ_USART3_TX    = LL_DMAMUX_REQ_USART3_TX,    /**< DMAMUX USART3 TX request   */

    DMA_REQ_UART4_RX     = LL_DMAMUX_REQ_UART4_RX,     /**< DMAMUX UART4 RX request    */
    DMA_REQ_UART4_TX     = LL_DMAMUX_REQ_UART4_TX,     /**< DMAMUX UART4 TX request    */
    DMA_REQ_UART5_RX     = LL_DMAMUX_REQ_UART5_RX,     /**< DMAMUX UART5 RX request    */
    DMA_REQ_UART5_TX     = LL_DMAMUX_REQ_UART5_TX,     /**< DMAMUX UART5 TX request    */

    DMA_REQ_LPUART1_RX   = LL_DMAMUX_REQ_LPUART1_RX,   /**< DMAMUX LPUART1 RX request  */
    DMA_REQ_LPUART1_TX   = LL_DMAMUX_REQ_LPUART1_TX,   /**< DMAMUX LPUART1 TX request  */

    DMA_REQ_SAI1_A       = LL_DMAMUX_REQ_SAI1_A,       /**< DMAMUX SAI1 A request      */
    DMA_REQ_SAI1_B       = LL_DMAMUX_REQ_SAI1_B,       /**< DMAMUX SAI1 B request      */

    DMA_REQ_AES_IN       = LL_DMAMUX_REQ_AES_IN,       /**< DMAMUX AES_IN request      */
    DMA_REQ_AES_OUT      = LL_DMAMUX_REQ_AES_OUT,      /**< DMAMUX AES_OUT request     */

    DMA_REQ_QSPI         = LL_DMAMUX_REQ_QSPI,         /**< DMAMUX Quad-SPI request    */

    DMA_REQ_FMAC_READ    = LL_DMAMUX_REQ_FMAC_READ,    /**< DMAMUX FMAC read request   */
    DMA_REQ_FMAC_WRITE   = LL_DMAMUX_REQ_FMAC_WRITE,   /**< DMAMUX FMAC write request  */

    DMA_REQ_CORDIC_READ  = LL_DMAMUX_REQ_CORDIC_READ,  /**< DMAMUX CORDIC read request */
    DMA_REQ_CORDIC_WRITE = LL_DMAMUX_REQ_CORDIC_WRITE, /**< DMAMUX CORDIC write request*/

    DMA_REQ_UCPD1_RX     = LL_DMAMUX_REQ_UCPD1_RX,     /**< DMAMUX UCPD RX request     */
    DMA_REQ_UCPD1_TX     = LL_DMAMUX_REQ_UCPD1_TX,     /**< DMAMUX UCPD TX request     */

}   dma_PeriphReqId_t;


/** DMA peripheral configuration structure */
typedef struct
{
    dma_PeriphId_t           DmaPeriphId;               /**< Specifies DMA number. Can be either DMA1 or DMA2
                                                             This parameter can be a value of \ref dma_Number_t */

    dma_ChannelId_t          DmaChannel;                /**< Specifies configured DMA channel.
                                                             This parameter can be a value of \ref dma_ChannelId_t */

    dma_Direction_t          Direction;                 /**< Specifies if the data will be transferred from memory to peripheral,
                                                             from memory to memory or from peripheral to memory.
                                                             This parameter can be a value of \ref dma_Direction_t */

    dma_TransferMode_t       TransferMode;              /**< Specifies the normal or circular operation mode.
                                                             This parameter can be a value of \ref dma_TransferMode_t
                                                             @note: The circular buffer mode cannot be used if the memory to memory
                                                             data transfer direction is configured on the selected Channel */

    dma_PeriphAddr_t         PeriphAddress;             /**< Specifies the peripheral base address for DMA transfer
                                                             or as Source base address in case of memory to memory transfer direction.
                                                             This parameter must be a value between Min_Data = 0 and Max_Data = 0xFFFFFFFF. */

    dma_MemoryAddr_t         MemoryAddress;             /**< Specifies the memory base address for DMA transfer
                                                             or as Destination base address in case of memory to memory transfer direction.
                                                             This parameter must be a value between Min_Data = 0 and Max_Data = 0xFFFFFFFF. */

    dma_PeriphAddrInc_t      PeriphAddrIncrement;       /**< Specifies whether the Peripheral address or Source address in case of memory to memory transfer direction
                                                             is incremented or not.
                                                             This parameter can be a value of \ref dma_PeriphAddrInc_t */

    dma_MemoryAddrInc_t      MemoryAddrIncrement;       /**< Specifies whether the Memory address or Destination address in case of memory to memory transfer direction
                                                             is incremented or not.
                                                             This parameter can be a value of \ref dma_MemoryAddrInc_t */

    dma_PeriphTransferSize_t PeriphTransferSize;        /**< Specifies the Peripheral data size alignment or Source data size alignment (byte, half word, word)
                                                             in case of memory to memory transfer direction.
                                                             This parameter can be a value of \ref dma_MemoryAddrInc_t */

    dma_MemoryTransferSize_t MemoryTransferSize;        /**< Specifies the Memory data size alignment or Destination data size alignment (byte, half word, word)
                                                             in case of memory to memory transfer direction.
                                                             This parameter can be a value of \ref dma_MemoryTransferSize_t */

    dma_DataCount_t          DataCount;                 /**< Specifies the number of data to transfer, in data unit.
                                                             The data unit is equal to the source buffer configuration set in PeripheralSize
                                                             or MemorySize parameters depending in the transfer direction.
                                                             This parameter must be a value between Min_Data = 0 and Max_Data = 65535 */

    dma_PeriphReqId_t        PeripheralReqId;           /**< Specifies the peripheral request.
                                                             This parameter can be a value of \ref dma_PeriphReqId_t */

    dma_Priority_t           Priority;                  /**< Specifies the channel priority level.
                                                             This parameter can be a value of \ref dma_Priority_t */

    dma_IsrCallback          TransferCompleteCallback;  /**< Pointer to user callback executed with transfer complete interrupt */

    dma_IsrCallback          HalfTransferCallback;      /**< Pointer to user callback executed with half transfer interrupt */

    dma_IsrCallback          TransferErrorCallback;     /**< Pointer to user callback executed with transfer error interrupt */

}   dma_ConfigStruct_t;

/* ========================== EXPORTED VARIABLES ============================ */

/* ========================= EXPORTED FUNCTIONS ============================= */


#endif /* DMA_DMA_TYPES_H */
