/**
 * \author Mr.Nobody
 * \file Test_Dma.c
 * \ingroup Dma
 * \brief Unit tests of Direct Memory Access (DMA) module.
 *
 * Dma.c is compiled unchanged with real LL drivers. DMA (incl. DMA_CSELR) and
 * DMAMUX1 registers are emulated by RegMem (host memory at MCU addresses), RCC
 * and NVIC modules are mocked by CMock.
 *
 * \note Emulated registers are plain memory. Writes to the flag clear register
 *       (IFCR) do not clear flags in ISR, tests check the value written to the
 *       clear register (last write). Channel enable bit (EN) is not cleared by
 *       hardware at the end of transfer.
 * \note STM32L4 / STM32L4+ DMA1 / DMA2 have 7 channels. STM32L4: peripheral
 *       request is selected by the 4-bit field of the channel in DMA_CSELR
 *       (fixed request mapping per channel, RM0394 / RM0351 DMA request
 *       tables). STM32L4+: peripheral request is routed by DMAMUX1, DMAMUX1
 *       channel of DMA2 channel 1 follows the last DMA1 channel (DMAMUX1
 *       channel 7). Expected registers are calculated independently of the
 *       module.
 * \note STM32L4 errata sheets not reviewed yet.
 */

/* ============================= INCLUDES =================================== */
#include <string.h>                         /* memset                         */
#include "unity.h"                          /* Unity testing framework        */
#include "RegMem.h"                         /* Register memory emulation      */
#include "CmsisHost.h"                      /* Core intrinsics emulation      */
#include "Dma_Port.h"                       /* Module under test              */
#include "MockRcc_Port.h"                   /* RCC module mock                */
#include "MockNvic_Port.h"                  /* NVIC module mock               */
#include "Stm32_dma.h"                      /* DMA registers definition       */
#if defined(DMAMUX1)
#include "Stm32_dmamux.h"                   /* DMAMUX registers definition    */
#endif
/* ============================= TYPEDEFS =================================== */

/* ======================= FORWARD DECLARATIONS ============================= */

static DMA_Channel_TypeDef *     Ut_Dma_Get_ChannelReg           ( uint32_t dmaIdx, uint32_t channelIdx );
#if defined(DMAMUX1)
static DMAMUX_Channel_TypeDef *  Ut_Dma_Get_MuxReg               ( uint32_t dmaIdx, uint32_t channelIdx );
#else
static DMA_Request_TypeDef *     Ut_Dma_Get_CselrReg             ( uint32_t dmaIdx );
#endif
static uint32_t                  Ut_Dma_Get_RequestReg           ( uint32_t dmaIdx, uint32_t channelIdx );
static void                      Ut_Dma_Set_RequestReg           ( uint32_t dmaIdx, uint32_t channelIdx, uint32_t regValue );
static DMA_TypeDef *             Ut_Dma_Get_DmaReg               ( uint32_t dmaIdx );
static void                      Ut_Dma_Expect_ClockState        ( rcc_PeriphId_t periphId, rcc_FunctionState_t clockState );
static void                      Ut_Dma_Expect_ClockActivation   ( rcc_PeriphId_t periphId );
static nvic_IsrCallback_t        Ut_Dma_Capture_IsrHandler       ( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, nvic_PeriphIrqList_t irqId );
static nvic_RequestState_t       Ut_Dma_Nvic_HandlerCallback     ( nvic_PeriphIrqList_t irqId, const nvic_IsrCallback_t irqHandler, int cmockNumCalls );
static void                      Ut_Dma_TransferCompleteCallback ( void );
static void                      Ut_Dma_HalfTransferCallback     ( void );
static void                      Ut_Dma_TransferErrorCallback    ( void );

/* ========================= SYMBOLIC CONSTANTS ============================= */

/** All flags of channel 1 (GIF, TCIF, HTIF, TEIF), other channels are shifted by 4 bits */
#define TEST_DMA_CHANNEL1_FLAGS             ( DMA_IFCR_CGIF1 | DMA_IFCR_CTCIF1 | DMA_IFCR_CHTIF1 | DMA_IFCR_CTEIF1 )

/** Distance of flags of consecutive channels in ISR / IFCR */
#define TEST_DMA_FLAG_SHIFT                 ( 4u )

/** Count of channels of one DMA peripheral (DMAMUX1 channels per DMA) */
#define TEST_DMA_CHANNELS_PER_DMA           ( 7u )

/** Distance of registers of consecutive DMA channels */
#define TEST_DMA_CHANNEL_REG_SIZE           ( DMA1_Channel2_BASE - DMA1_Channel1_BASE )

#if defined(DMAMUX1)
/** Distance of registers of consecutive DMAMUX1 channels */
#define TEST_DMA_MUX_REG_SIZE               ( DMAMUX1_Channel1_BASE - DMAMUX1_Channel0_BASE )

/** Request identification above the last DMAMUX1 request (RM0432 DMAMUX table) */
#define TEST_DMA_INVALID_REQUEST            ( (dma_PeriphReqId_t)( LL_DMAMUX_REQ_HASH_IN + 1u ) )

/** Request register value of USART2 RX (DMAMUX1 request) */
#define TEST_DMA_USART2_RX_REG              ( LL_DMAMUX_REQ_USART2_RX )

/** Request register value of SPI1 TX (DMAMUX1 request) */
#define TEST_DMA_SPI1_TX_REG                ( LL_DMAMUX_REQ_SPI1_TX )
#else
/** Width of request selection field (CxS) of one channel in DMA_CSELR */
#define TEST_DMA_CSELR_FIELD_WIDTH          ( 4u )

/** Request selection field (CxS) of channel 1 in DMA_CSELR */
#define TEST_DMA_CSELR_FIELD_MASK           ( 0xFu )

/** Request identification above the last request of the device */
#define TEST_DMA_INVALID_REQUEST            ( DMA_REQ_CNT )

/** Request register value of USART2 RX on DMA1 channel 6 (C6S, RM0394 DMA1 requests) */
#define TEST_DMA_USART2_RX_REG              ( 2u )

/** Request register value of SPI1 TX on DMA1 channel 3 (C3S, RM0394 DMA1 requests) */
#define TEST_DMA_SPI1_TX_REG                ( 1u )
#endif /* DMAMUX1 */

/** Peripheral (source) address used by tests */
#define TEST_DMA_PERIPH_ADDR                ( 0x20000100u )

/** Memory (destination) address used by tests */
#define TEST_DMA_MEMORY_ADDR                ( 0x20000200u )

/** Index of the last channel of DMA peripheral */
#define TEST_DMA_LAST_CHANNEL               ( (dma_ChannelId_t)( DMA_CHANNEL_CNT - 1u ) )

/* ============================== MACROS ==================================== */

/** Count of items of an array */
#define TEST_DMA_ARRAY_CNT( array )         ( sizeof( array ) / sizeof( ( array )[ 0u ] ) )

#if defined(DMAMUX1)
/** Expects read of DMAMUX1 clock state (STM32L4+ only) */
#define UT_DMA_EXPECT_MUX_CLOCK_STATE( clockState )  Ut_Dma_Expect_ClockState( RCC_PERIPH_DMAMUX1, ( clockState ) )

/** Expects activation of inactive DMAMUX1 clock (STM32L4+ only) */
#define UT_DMA_EXPECT_MUX_CLOCK_ACTIVATION()         Ut_Dma_Expect_ClockActivation( RCC_PERIPH_DMAMUX1 )
#else
/** STM32L4 has no DMAMUX1 - no clock expectation */
#define UT_DMA_EXPECT_MUX_CLOCK_STATE( clockState )  (void)( clockState )

/** STM32L4 has no DMAMUX1 - no clock expectation */
#define UT_DMA_EXPECT_MUX_CLOCK_ACTIVATION()         do { } while( 0 )
#endif /* DMAMUX1 */

/* ========================== LOCAL VARIABLES =============================== */

/** NVIC interrupt of every DMA peripheral and channel */
static const nvic_PeriphIrqList_t utDma_IrqLut[ DMA_PERIPH_CNT ][ DMA_CHANNEL_CNT ] =
{
    {
        NVIC_PERIPH_IRQ_DMA1_CHANNEL1, NVIC_PERIPH_IRQ_DMA1_CHANNEL2, NVIC_PERIPH_IRQ_DMA1_CHANNEL3,
        NVIC_PERIPH_IRQ_DMA1_CHANNEL4, NVIC_PERIPH_IRQ_DMA1_CHANNEL5, NVIC_PERIPH_IRQ_DMA1_CHANNEL6,
#if defined(DMA1_Channel7)
        NVIC_PERIPH_IRQ_DMA1_CHANNEL7,
#endif
#if defined(DMA1_Channel8)
        NVIC_PERIPH_IRQ_DMA1_CHANNEL8,
#endif
    },
    {
        NVIC_PERIPH_IRQ_DMA2_CHANNEL1, NVIC_PERIPH_IRQ_DMA2_CHANNEL2, NVIC_PERIPH_IRQ_DMA2_CHANNEL3,
        NVIC_PERIPH_IRQ_DMA2_CHANNEL4, NVIC_PERIPH_IRQ_DMA2_CHANNEL5, NVIC_PERIPH_IRQ_DMA2_CHANNEL6,
#if defined(DMA1_Channel7)
        NVIC_PERIPH_IRQ_DMA2_CHANNEL7,
#endif
#if defined(DMA1_Channel8)
        NVIC_PERIPH_IRQ_DMA2_CHANNEL8,
#endif
    },
};

/** Interrupt handler of the channel registered to NVIC mock */
static nvic_IsrCallback_t utDma_CapturedIsr = NULL;

/** Count of executed callbacks (sequence counter) */
static uint32_t utDma_CallbackCnt = 0u;

/** Sequence number of last transfer complete callback execution (0 - not executed) */
static uint32_t utDma_TransferCompleteSeq = 0u;

/** Sequence number of last half transfer callback execution (0 - not executed) */
static uint32_t utDma_HalfTransferSeq = 0u;

/** Sequence number of last transfer error callback execution (0 - not executed) */
static uint32_t utDma_TransferErrorSeq = 0u;

/* ============================ TEST FIXTURE ================================ */

void setUp( void )
{
    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Reset() );

    utDma_CapturedIsr         = NULL;
    utDma_CallbackCnt         = 0u;
    utDma_TransferCompleteSeq = 0u;
    utDma_HalfTransferSeq     = 0u;
    utDma_TransferErrorSeq    = 0u;
}


void tearDown( void )
{
    /* Mocks are verified by generated runner */
}

/* ========================== MODULE VERSION ================================ */

/**
 * \brief   Dma_Get_ModuleVersion() returns version of the module.
 *
 * \details Reads the module version structure.
 *
 * \par Expected results
 * - Version is 1.0.0 (Major 1, Minor 0, Patch 0).
 */
void Ut_Dma_Get_ModuleVersion_ReturnsVersion( void )
{
    dma_ModuleVersion_t version = Dma_Get_ModuleVersion();

    TEST_ASSERT_EQUAL_UINT8( 1u, version.Major );
    TEST_ASSERT_EQUAL_UINT8( 0u, version.Minor );
    TEST_ASSERT_EQUAL_UINT8( 0u, version.Patch );
}

/* ========================= DEFAULT CONFIGURATION ========================== */

/**
 * \brief   Dma_Get_DefaultConfig() fills default configuration.
 *
 * \details Configuration structure is pre-filled with non default values.
 *
 * \par Expected results
 * - DMA1 channel 1, peripheral to memory, normal mode, static addresses 0,
 *   8 bit transfers, 0 data, no peripheral request (MEM2MEM), low priority,
 *   no callbacks.
 */
void Ut_Dma_Get_DefaultConfig_FillsDefaults( void )
{
    dma_ConfigStruct_t dmaConfig;

    (void)memset( &dmaConfig, 0xA5, sizeof( dmaConfig ) );

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Get_DefaultConfig( &dmaConfig ) );

    TEST_ASSERT_EQUAL( DMA_PERIPH_1,               dmaConfig.DmaPeriphId );
    TEST_ASSERT_EQUAL( DMA_CHANNEL_1,              dmaConfig.DmaChannel );
    TEST_ASSERT_EQUAL( DMA_DIR_PERIPH_TO_MEMORY,   dmaConfig.Direction );
    TEST_ASSERT_EQUAL( DMA_TRANSFER_MODE_NORMAL,   dmaConfig.TransferMode );
    TEST_ASSERT_EQUAL_HEX32( 0u,                   dmaConfig.PeriphAddress );
    TEST_ASSERT_EQUAL_HEX32( 0u,                   dmaConfig.MemoryAddress );
    TEST_ASSERT_EQUAL( DMA_PERIPH_ADDR_STATIC,     dmaConfig.PeriphAddrIncrement );
    TEST_ASSERT_EQUAL( DMA_MEMORY_ADDR_STATIC,     dmaConfig.MemoryAddrIncrement );
    TEST_ASSERT_EQUAL( DMA_TRANSFER_SIZE_8BIT,     dmaConfig.PeriphTransferSize );
    TEST_ASSERT_EQUAL( DMA_TRANSFER_SIZE_8BIT,     dmaConfig.MemoryTransferSize );
    TEST_ASSERT_EQUAL_UINT32( 0u,                  dmaConfig.DataCount );
    TEST_ASSERT_EQUAL( DMA_REQ_MEM2MEM,            dmaConfig.PeripheralReqId );
    TEST_ASSERT_EQUAL( DMA_PRIORITY_LOW,           dmaConfig.Priority );
    TEST_ASSERT_NULL( dmaConfig.TransferCompleteCallback );
    TEST_ASSERT_NULL( dmaConfig.HalfTransferCallback );
    TEST_ASSERT_NULL( dmaConfig.TransferErrorCallback );
}


/**
 * \brief   Dma_Get_DefaultConfig() rejects null pointer.
 *
 * \par Expected results
 * - DMA_REQUEST_ERROR is returned.
 */
void Ut_Dma_Get_DefaultConfig_NullPointer_ReturnsError( void )
{
    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Get_DefaultConfig( NULL ) );
}

/* ============================ INITIALIZATION ============================== */

/**
 * \brief   Dma_Init() activates inactive clocks and configures the channel.
 *
 * \details DMA2 channel 1, memory to memory, both addresses incremented, 32 bit
 *          transfers, 16 data, very high priority, all callbacks configured.
 *
 * \par Expected results
 * - RCC clocks of DMA2 (and DMAMUX1 on STM32L4+) are read and activated.
 * - CCR contains MEM2MEM, increments, sizes and priority, channel disabled.
 * - CPAR, CMAR, CNDTR hold source, destination and data count.
 * - Request register of DMA2 channel 1 (DMAMUX1 channel / C1S field of
 *   DMA2_CSELR) holding previous request selects no request (MEM2MEM = 0).
 * - Callbacks are stored.
 */
void Ut_Dma_Init_ClockInactive_ActivatesClocksAndConfiguresChannel( void )
{
    dma_ConfigStruct_t          dmaConfig;
    dma_IsrCallback             callback   = NULL;
    DMA_Channel_TypeDef * const channelReg = Ut_Dma_Get_ChannelReg( 1u, 0u );

    (void)Dma_Get_DefaultConfig( &dmaConfig );
    dmaConfig.DmaPeriphId              = DMA_PERIPH_2;
    dmaConfig.DmaChannel               = DMA_CHANNEL_1;
    dmaConfig.Direction                = DMA_DIR_MEMORY_TO_MEMORY;
    dmaConfig.PeriphAddress            = TEST_DMA_PERIPH_ADDR;
    dmaConfig.MemoryAddress            = TEST_DMA_MEMORY_ADDR;
    dmaConfig.PeriphAddrIncrement      = DMA_PERIPH_ADDR_INCREMENT;
    dmaConfig.MemoryAddrIncrement      = DMA_MEMORY_ADDR_INCREMENT;
    dmaConfig.PeriphTransferSize       = DMA_TRANSFER_SIZE_32BIT;
    dmaConfig.MemoryTransferSize       = DMA_TRANSFER_SIZE_32BIT;
    dmaConfig.DataCount                = 16u;
    dmaConfig.PeripheralReqId          = DMA_REQ_MEM2MEM;
    dmaConfig.Priority                 = DMA_PRIORITY_VERYHIGH;
    dmaConfig.TransferCompleteCallback = Ut_Dma_TransferCompleteCallback;
    dmaConfig.HalfTransferCallback     = Ut_Dma_HalfTransferCallback;
    dmaConfig.TransferErrorCallback    = Ut_Dma_TransferErrorCallback;

    Ut_Dma_Set_RequestReg( 1u, 0u, 3u );  /* Previous request */

    Ut_Dma_Expect_ClockActivation( RCC_PERIPH_DMA2 );
    UT_DMA_EXPECT_MUX_CLOCK_ACTIVATION();

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Init( &dmaConfig ) );

    TEST_ASSERT_EQUAL_HEX32( DMA_CCR_MEM2MEM | DMA_CCR_PINC | DMA_CCR_MINC | DMA_CCR_PSIZE_1 |
                             DMA_CCR_MSIZE_1 | DMA_CCR_PL, channelReg->CCR );
    TEST_ASSERT_EQUAL_HEX32( TEST_DMA_PERIPH_ADDR, channelReg->CPAR );
    TEST_ASSERT_EQUAL_HEX32( TEST_DMA_MEMORY_ADDR, channelReg->CMAR );
    TEST_ASSERT_EQUAL_UINT32( 16u, channelReg->CNDTR );
    TEST_ASSERT_EQUAL_HEX32( 0u, Ut_Dma_Get_RequestReg( 1u, 0u ) );

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Get_TransferCompleteIsrHandler( DMA_PERIPH_2, DMA_CHANNEL_1, &callback ) );
    TEST_ASSERT_EQUAL_PTR( Ut_Dma_TransferCompleteCallback, callback );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Get_HalfTransferIsrHandler( DMA_PERIPH_2, DMA_CHANNEL_1, &callback ) );
    TEST_ASSERT_EQUAL_PTR( Ut_Dma_HalfTransferCallback, callback );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Get_TransferErrorIsrHandler( DMA_PERIPH_2, DMA_CHANNEL_1, &callback ) );
    TEST_ASSERT_EQUAL_PTR( Ut_Dma_TransferErrorCallback, callback );
}


/**
 * \brief   Dma_Init() does not activate already active clocks.
 *
 * \details RCC mock reports active DMA1 (and DMAMUX1 on STM32L4+) clock, default
 *          configuration of DMA1 channel 1 is initialized.
 *
 * \par Expected results
 * - Only clock states are read, Rcc_Set_PeriphActive() is not called.
 * - DMA_REQUEST_OK is returned.
 */
void Ut_Dma_Init_ClockActive_DoesNotReactivate( void )
{
    dma_ConfigStruct_t dmaConfig;

    (void)Dma_Get_DefaultConfig( &dmaConfig );

    Ut_Dma_Expect_ClockState( RCC_PERIPH_DMA1,    RCC_FUNCTION_ACTIVE );
    UT_DMA_EXPECT_MUX_CLOCK_STATE( RCC_FUNCTION_ACTIVE );

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Init( &dmaConfig ) );
}


/**
 * \brief   Dma_Init() routes peripheral request and configures different transfer sizes.
 *
 * \details DMA1 channel 6, USART2 RX request (served by DMA1 channel 6 on
 *          STM32L4), 8 bit peripheral, 32 bit memory.
 *
 * \par Expected results
 * - Request register of DMA1 channel 6 selects USART2 RX request (DMAMUX1
 *   channel 5 on STM32L4+, C6S = 2 on STM32L4).
 * - CCR contains PSIZE byte and MSIZE word (packing done by the channel).
 */
void Ut_Dma_Init_PeripheralRequest_Selected( void )
{
    dma_ConfigStruct_t dmaConfig;

    (void)Dma_Get_DefaultConfig( &dmaConfig );
    dmaConfig.DmaChannel         = DMA_CHANNEL_6;
    dmaConfig.PeripheralReqId    = DMA_REQ_USART2_RX;
    dmaConfig.PeriphTransferSize = DMA_TRANSFER_SIZE_8BIT;
    dmaConfig.MemoryTransferSize = DMA_TRANSFER_SIZE_32BIT;

    Ut_Dma_Expect_ClockState( RCC_PERIPH_DMA1,    RCC_FUNCTION_ACTIVE );
    UT_DMA_EXPECT_MUX_CLOCK_STATE( RCC_FUNCTION_ACTIVE );

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Init( &dmaConfig ) );

    TEST_ASSERT_EQUAL_HEX32( TEST_DMA_USART2_RX_REG, Ut_Dma_Get_RequestReg( 0u, 5u ) );
    TEST_ASSERT_EQUAL_HEX32( DMA_CCR_MSIZE_1, Ut_Dma_Get_ChannelReg( 0u, 5u )->CCR & ( DMA_CCR_PSIZE | DMA_CCR_MSIZE ) );
}


/**
 * \brief   Dma_Init() disables enabled channel before configuration.
 *
 * \details Channel is enabled with transfer complete interrupt.
 *
 * \par Expected results
 * - DMA_REQUEST_OK, channel is disabled (EN cleared), interrupt enable kept.
 * - Channel configuration is written (CPAR).
 */
void Ut_Dma_Init_ChannelEnabled_DisablesChannelFirst( void )
{
    dma_ConfigStruct_t          dmaConfig;
    DMA_Channel_TypeDef * const channelReg = Ut_Dma_Get_ChannelReg( 0u, 0u );

    (void)Dma_Get_DefaultConfig( &dmaConfig );
    dmaConfig.PeriphAddress = TEST_DMA_PERIPH_ADDR;
    channelReg->CCR         = DMA_CCR_EN | DMA_CCR_TCIE;

    Ut_Dma_Expect_ClockState( RCC_PERIPH_DMA1,    RCC_FUNCTION_ACTIVE );
    UT_DMA_EXPECT_MUX_CLOCK_STATE( RCC_FUNCTION_ACTIVE );

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Init( &dmaConfig ) );

    TEST_ASSERT_EQUAL_HEX32( 0u, channelReg->CCR & DMA_CCR_EN );
    TEST_ASSERT_EQUAL_HEX32( DMA_CCR_TCIE, channelReg->CCR & DMA_CCR_TCIE );
    TEST_ASSERT_EQUAL_HEX32( TEST_DMA_PERIPH_ADDR, channelReg->CPAR );
}


/**
 * \brief   Dma_Init() accepts memory to memory direction on DMA1.
 *
 * \details STM32L4 DMA1 supports memory to memory transfer (MEM2MEM).
 *
 * \par Expected results
 * - DMA_REQUEST_OK, MEM2MEM set in CCR of DMA1 channel 1.
 */
void Ut_Dma_Init_MemToMemOnDma1_Accepted( void )
{
    dma_ConfigStruct_t dmaConfig;

    (void)Dma_Get_DefaultConfig( &dmaConfig );
    dmaConfig.Direction = DMA_DIR_MEMORY_TO_MEMORY;

    Ut_Dma_Expect_ClockState( RCC_PERIPH_DMA1,    RCC_FUNCTION_ACTIVE );
    UT_DMA_EXPECT_MUX_CLOCK_STATE( RCC_FUNCTION_ACTIVE );

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Init( &dmaConfig ) );
    TEST_ASSERT_EQUAL_HEX32( DMA_CCR_MEM2MEM, Ut_Dma_Get_ChannelReg( 0u, 0u )->CCR & ( DMA_CCR_MEM2MEM | DMA_CCR_DIR ) );
}


/**
 * \brief   Dma_Init() rejects data count above 16 bit CNDTR range.
 *
 * \par Expected results
 * - DMA_REQUEST_ERROR is returned.
 */
void Ut_Dma_Init_DataCountOutOfRange_ReturnsError( void )
{
    dma_ConfigStruct_t dmaConfig;

    (void)Dma_Get_DefaultConfig( &dmaConfig );
    dmaConfig.DataCount = DMA_DATA_COUNT_MAX + 1u;

    Ut_Dma_Expect_ClockState( RCC_PERIPH_DMA1,    RCC_FUNCTION_ACTIVE );
    UT_DMA_EXPECT_MUX_CLOCK_STATE( RCC_FUNCTION_ACTIVE );

    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Init( &dmaConfig ) );
}


/**
 * \brief   Dma_Init() rejects invalid value of every configuration parameter.
 *
 * \details Default configuration with one invalid parameter: peripheral request
 *          above the last request of the device, request not served by DMA1
 *          channel 1 (STM32L4 only - USART2 TX is served by channel 7),
 *          transfer mode, direction, priority, peripheral and memory increment,
 *          peripheral and memory transfer size.
 *
 * \par Expected results
 * - DMA_REQUEST_ERROR for every invalid parameter (clocks are read every time).
 */
void Ut_Dma_Init_InvalidParameter_ReturnsError( void )
{
    dma_ConfigStruct_t dmaConfig;

    for( uint32_t paramIdx = 0u; 9u > paramIdx; paramIdx++ )
    {
        (void)Dma_Get_DefaultConfig( &dmaConfig );

        if( 0u == paramIdx )
        {
            dmaConfig.PeripheralReqId = TEST_DMA_INVALID_REQUEST;
        }
        else if( 1u == paramIdx )
        {
#if defined(DMAMUX1)
            dmaConfig.PeripheralReqId = TEST_DMA_INVALID_REQUEST;
#else
            dmaConfig.PeripheralReqId = DMA_REQ_USART2_TX;
#endif
        }
        else if( 2u == paramIdx )
        {
            dmaConfig.TransferMode = (dma_TransferMode_t)DMA_CCR_EN;
        }
        else if( 3u == paramIdx )
        {
            dmaConfig.Direction = (dma_Direction_t)DMA_CCR_EN;
        }
        else if( 4u == paramIdx )
        {
            dmaConfig.Priority = DMA_PRIORITY_CNT;
        }
        else if( 5u == paramIdx )
        {
            dmaConfig.PeriphAddrIncrement = (dma_PeriphAddrInc_t)DMA_CCR_EN;
        }
        else if( 6u == paramIdx )
        {
            dmaConfig.MemoryAddrIncrement = (dma_MemoryAddrInc_t)DMA_CCR_EN;
        }
        else if( 7u == paramIdx )
        {
            dmaConfig.PeriphTransferSize = DMA_TRANSFER_SIZE_CNT;
        }
        else
        {
            dmaConfig.MemoryTransferSize = DMA_TRANSFER_SIZE_CNT;
        }

        Ut_Dma_Expect_ClockState( RCC_PERIPH_DMA1,    RCC_FUNCTION_ACTIVE );
        UT_DMA_EXPECT_MUX_CLOCK_STATE( RCC_FUNCTION_ACTIVE );

        TEST_ASSERT_EQUAL_MESSAGE( DMA_REQUEST_ERROR, Dma_Init( &dmaConfig ), "Invalid parameter accepted" );
    }
}


/**
 * \brief   Dma_Init() reports error of clock state read.
 *
 * \par Expected results
 * - DMA_REQUEST_ERROR is returned, clock is not activated (strict mock).
 */
void Ut_Dma_Init_RccStateError_ReturnsError( void )
{
    dma_ConfigStruct_t dmaConfig;

    (void)Dma_Get_DefaultConfig( &dmaConfig );

    Rcc_Get_PeriphState_ExpectAndReturn( RCC_PERIPH_DMA1, NULL, RCC_REQUEST_ERROR );
    Rcc_Get_PeriphState_IgnoreArg_funcState();

    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Init( &dmaConfig ) );
}


/**
 * \brief   Dma_Init() reports error of DMA clock activation.
 *
 * \par Expected results
 * - DMA_REQUEST_ERROR is returned, DMAMUX1 clock is not handled, channel is not
 *   configured.
 */
void Ut_Dma_Init_RccActivationError_ReturnsError( void )
{
    dma_ConfigStruct_t dmaConfig;

    (void)Dma_Get_DefaultConfig( &dmaConfig );
    dmaConfig.PeriphAddress = TEST_DMA_PERIPH_ADDR;

    Ut_Dma_Expect_ClockState( RCC_PERIPH_DMA1, RCC_FUNCTION_INACTIVE );
    Rcc_Set_PeriphActive_ExpectAndReturn( RCC_PERIPH_DMA1, RCC_REQUEST_ERROR );

    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Init( &dmaConfig ) );
    TEST_ASSERT_EQUAL_HEX32( 0u, Ut_Dma_Get_ChannelReg( 0u, 0u )->CPAR );
}


/**
 * \brief   Dma_Init() reports error of DMAMUX1 clock activation (STM32L4+).
 *
 * \details DMA1 clock is active, DMAMUX1 clock is inactive and its activation fails.
 *
 * \par Expected results
 * - DMA_REQUEST_ERROR is returned, channel is not configured.
 */
void Ut_Dma_Init_DmamuxClockError_ReturnsError( void )
{
#if defined(DMAMUX1)
    dma_ConfigStruct_t dmaConfig;

    (void)Dma_Get_DefaultConfig( &dmaConfig );
    dmaConfig.PeriphAddress = TEST_DMA_PERIPH_ADDR;

    Ut_Dma_Expect_ClockState( RCC_PERIPH_DMA1,    RCC_FUNCTION_ACTIVE );
    Ut_Dma_Expect_ClockState( RCC_PERIPH_DMAMUX1, RCC_FUNCTION_INACTIVE );
    Rcc_Set_PeriphActive_ExpectAndReturn( RCC_PERIPH_DMAMUX1, RCC_REQUEST_ERROR );

    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Init( &dmaConfig ) );
    TEST_ASSERT_EQUAL_HEX32( 0u, Ut_Dma_Get_ChannelReg( 0u, 0u )->CPAR );
#else
    TEST_IGNORE_MESSAGE( "MCU without DMAMUX1" );
#endif /* DMAMUX1 */
}


/**
 * \brief   Dma_Init() rejects null pointer and invalid identifiers.
 *
 * \par Expected results
 * - DMA_REQUEST_ERROR is returned, RCC is not called (strict mock).
 */
void Ut_Dma_Init_InvalidConfig_ReturnsErrorWithoutRccAccess( void )
{
    dma_ConfigStruct_t dmaConfig;

    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Init( NULL ) );

    (void)Dma_Get_DefaultConfig( &dmaConfig );
    dmaConfig.DmaPeriphId = DMA_PERIPH_CNT;
    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Init( &dmaConfig ) );

    (void)Dma_Get_DefaultConfig( &dmaConfig );
    dmaConfig.DmaChannel = DMA_CHANNEL_CNT;
    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Init( &dmaConfig ) );
}

/* ========================= TRANSFER ACTIVATION ============================ */

/**
 * \brief   Dma_Set_TransferActive() clears channel flags and enables channel.
 *
 * \details Every channel of both DMA peripherals.
 *
 * \par Expected results
 * - DMA_REQUEST_OK, EN set.
 * - All four flags of the channel (incl. GIF - channel disabled) written to IFCR
 *   at channel offset ( 4 x channel index ).
 */
void Ut_Dma_Set_TransferActive_AllChannels_ClearsFlagsAndEnables( void )
{
    for( uint32_t dmaIdx = 0u; DMA_PERIPH_CNT > dmaIdx; dmaIdx++ )
    {
        for( uint32_t channelIdx = 0u; DMA_CHANNEL_CNT > channelIdx; channelIdx++ )
        {
            TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Reset() );

            TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_TransferActive( (dma_PeriphId_t)dmaIdx, (dma_ChannelId_t)channelIdx ) );

            TEST_ASSERT_EQUAL_HEX32( DMA_CCR_EN, Ut_Dma_Get_ChannelReg( dmaIdx, channelIdx )->CCR & DMA_CCR_EN );
            TEST_ASSERT_EQUAL_HEX32( TEST_DMA_CHANNEL1_FLAGS << ( channelIdx * TEST_DMA_FLAG_SHIFT ), Ut_Dma_Get_DmaReg( dmaIdx )->IFCR );
        }
    }
}


/**
 * \brief   Dma_Set_TransferActive() reports transfer error.
 *
 * \details Transfer error flag of DMA1 channel 3 stays active after enable
 *          (emulated register is not cleared by IFCR write).
 *
 * \par Expected results
 * - DMA_REQUEST_ERROR is returned.
 */
void Ut_Dma_Set_TransferActive_TransferError_ReturnsError( void )
{
    DMA1->ISR = DMA_ISR_TEIF3;

    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_TransferActive( DMA_PERIPH_1, DMA_CHANNEL_3 ) );
}


/**
 * \brief   Dma_Set_TransferActive() refuses circular mode in memory to memory direction.
 *
 * \details DMA2 channel 2: MEM2MEM and CIRC, then MEM2MEM only, then CIRC only.
 *
 * \par Expected results
 * - MEM2MEM + CIRC: DMA_REQUEST_ERROR, channel not enabled, flags not cleared.
 * - MEM2MEM only and CIRC only: DMA_REQUEST_OK, channel enabled.
 */
void Ut_Dma_Set_TransferActive_MemToMemCircular_Refused( void )
{
    DMA_Channel_TypeDef * const channelReg = Ut_Dma_Get_ChannelReg( 1u, 1u );

    channelReg->CCR = DMA_CCR_MEM2MEM | DMA_CCR_CIRC;
    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_TransferActive( DMA_PERIPH_2, DMA_CHANNEL_2 ) );
    TEST_ASSERT_EQUAL_HEX32( 0u, channelReg->CCR & DMA_CCR_EN );
    TEST_ASSERT_EQUAL_HEX32( 0u, DMA2->IFCR );

    channelReg->CCR = DMA_CCR_MEM2MEM;
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_TransferActive( DMA_PERIPH_2, DMA_CHANNEL_2 ) );
    TEST_ASSERT_EQUAL_HEX32( DMA_CCR_EN, channelReg->CCR & DMA_CCR_EN );

    channelReg->CCR = DMA_CCR_CIRC;
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_TransferActive( DMA_PERIPH_2, DMA_CHANNEL_2 ) );
    TEST_ASSERT_EQUAL_HEX32( DMA_CCR_EN, channelReg->CCR & DMA_CCR_EN );
}


/**
 * \brief   Dma_Set_TransferInactive() disables channel and clears its flags.
 *
 * \details DMA1 channel 1 is enabled with transfer complete and half transfer
 *          interrupts.
 *
 * \par Expected results
 * - DMA_REQUEST_OK, EN cleared, interrupt enables restored.
 * - All flags of channel 1 (incl. GIF - channel already disabled) written to IFCR.
 */
void Ut_Dma_Set_TransferInactive_EnabledChannel_DisablesAndClearsFlags( void )
{
    DMA_Channel_TypeDef * const channelReg = Ut_Dma_Get_ChannelReg( 0u, 0u );

    channelReg->CCR = DMA_CCR_EN | DMA_CCR_TCIE | DMA_CCR_HTIE;

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_TransferInactive( DMA_PERIPH_1, DMA_CHANNEL_1 ) );

    TEST_ASSERT_EQUAL_HEX32( DMA_CCR_TCIE | DMA_CCR_HTIE, channelReg->CCR );
    TEST_ASSERT_EQUAL_HEX32( TEST_DMA_CHANNEL1_FLAGS, DMA1->IFCR );
}


/**
 * \brief   Dma_Get_TransferState() reports channel enable state.
 *
 * \details Last channel of DMA2.
 *
 * \par Expected results
 * - EN set: DMA_FUNCTION_ACTIVE, EN cleared: DMA_FUNCTION_INACTIVE.
 */
void Ut_Dma_Get_TransferState_ReportsEnableBit( void )
{
    dma_FunctionState_t         state      = DMA_FUNCTION_INACTIVE;
    DMA_Channel_TypeDef * const channelReg = Ut_Dma_Get_ChannelReg( 1u, TEST_DMA_LAST_CHANNEL );

    channelReg->CCR = DMA_CCR_EN;
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Get_TransferState( DMA_PERIPH_2, TEST_DMA_LAST_CHANNEL, &state ) );
    TEST_ASSERT_EQUAL( DMA_FUNCTION_ACTIVE, state );

    channelReg->CCR = 0u;
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Get_TransferState( DMA_PERIPH_2, TEST_DMA_LAST_CHANNEL, &state ) );
    TEST_ASSERT_EQUAL( DMA_FUNCTION_INACTIVE, state );
}


/**
 * \brief   Channel activation functions control the channel like transfer
 *          activation functions.
 *
 * \par Expected results
 * - Dma_Set_ChannelActive() sets EN, Dma_Get_ChannelState() reports active.
 * - Dma_Set_ChannelInactive() clears EN, Dma_Get_ChannelState() reports inactive.
 */
void Ut_Dma_Set_ChannelActive_ControlsChannelEnable( void )
{
    dma_FunctionState_t state = DMA_FUNCTION_INACTIVE;

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_ChannelActive( DMA_PERIPH_1, DMA_CHANNEL_4 ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Get_ChannelState( DMA_PERIPH_1, DMA_CHANNEL_4, &state ) );
    TEST_ASSERT_EQUAL( DMA_FUNCTION_ACTIVE, state );

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_ChannelInactive( DMA_PERIPH_1, DMA_CHANNEL_4 ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Get_ChannelState( DMA_PERIPH_1, DMA_CHANNEL_4, &state ) );
    TEST_ASSERT_EQUAL( DMA_FUNCTION_INACTIVE, state );
}

/* ======================= CHANNEL CONFIGURATION ============================ */

/**
 * \brief   Transfer direction is written to CCR and read back.
 *
 * \details All directions on DMA1 and DMA2 channel 2.
 *
 * \par Expected results
 * - DIR / MEM2MEM hold requested direction, getter returns it.
 */
void Ut_Dma_Set_Direction_AllDirections_RoundTrip( void )
{
    static const dma_Direction_t directionLut[] = { DMA_DIR_MEMORY_TO_PERIPH, DMA_DIR_MEMORY_TO_MEMORY, DMA_DIR_PERIPH_TO_MEMORY };
    dma_Direction_t direction = DMA_DIR_PERIPH_TO_MEMORY;

    for( uint32_t dmaIdx = 0u; DMA_PERIPH_CNT > dmaIdx; dmaIdx++ )
    {
        for( uint32_t idx = 0u; TEST_DMA_ARRAY_CNT( directionLut ) > idx; idx++ )
        {
            TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_Direction( (dma_PeriphId_t)dmaIdx, DMA_CHANNEL_2, directionLut[ idx ] ) );
            TEST_ASSERT_EQUAL_HEX32( directionLut[ idx ], Ut_Dma_Get_ChannelReg( dmaIdx, 1u )->CCR & ( DMA_CCR_DIR | DMA_CCR_MEM2MEM ) );

            TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Get_Direction( (dma_PeriphId_t)dmaIdx, DMA_CHANNEL_2, &direction ) );
            TEST_ASSERT_EQUAL( directionLut[ idx ], direction );
        }
    }

    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_Direction( DMA_PERIPH_1, DMA_CHANNEL_2, (dma_Direction_t)DMA_CCR_EN ) );
}


/**
 * \brief   Transfer mode is written to CCR and read back.
 *
 * \par Expected results
 * - CIRC set for circular mode and cleared for normal mode.
 */
void Ut_Dma_Set_TransferMode_RoundTrip( void )
{
    dma_TransferMode_t          mode       = DMA_TRANSFER_MODE_NORMAL;
    DMA_Channel_TypeDef * const channelReg = Ut_Dma_Get_ChannelReg( 0u, 5u );

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_TransferMode( DMA_PERIPH_1, DMA_CHANNEL_6, DMA_TRANSFER_MODE_CIRCULAR ) );
    TEST_ASSERT_EQUAL_HEX32( DMA_CCR_CIRC, channelReg->CCR );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Get_TransferMode( DMA_PERIPH_1, DMA_CHANNEL_6, &mode ) );
    TEST_ASSERT_EQUAL( DMA_TRANSFER_MODE_CIRCULAR, mode );

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_TransferMode( DMA_PERIPH_1, DMA_CHANNEL_6, DMA_TRANSFER_MODE_NORMAL ) );
    TEST_ASSERT_EQUAL_HEX32( 0u, channelReg->CCR );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Get_TransferMode( DMA_PERIPH_1, DMA_CHANNEL_6, &mode ) );
    TEST_ASSERT_EQUAL( DMA_TRANSFER_MODE_NORMAL, mode );

    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_TransferMode( DMA_PERIPH_1, DMA_CHANNEL_6, (dma_TransferMode_t)DMA_CCR_EN ) );
}


/**
 * \brief   Peripheral and memory addresses are written to CPAR / CMAR.
 *
 * \par Expected results
 * - CPAR holds peripheral address, CMAR memory address, getters return them
 *   (memory address getter reads CMAR, not CPAR).
 */
void Ut_Dma_Set_Addresses_RoundTrip( void )
{
    dma_PeriphAddr_t            periphAddr = 0u;
    dma_MemoryAddr_t            memoryAddr = 0u;
    DMA_Channel_TypeDef * const channelReg = Ut_Dma_Get_ChannelReg( 1u, 4u );

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_PeriphAddr( DMA_PERIPH_2, DMA_CHANNEL_5, TEST_DMA_PERIPH_ADDR ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_MemoryAddr( DMA_PERIPH_2, DMA_CHANNEL_5, TEST_DMA_MEMORY_ADDR ) );

    TEST_ASSERT_EQUAL_HEX32( TEST_DMA_PERIPH_ADDR, channelReg->CPAR );
    TEST_ASSERT_EQUAL_HEX32( TEST_DMA_MEMORY_ADDR, channelReg->CMAR );

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Get_PeriphAddr( DMA_PERIPH_2, DMA_CHANNEL_5, &periphAddr ) );
    TEST_ASSERT_EQUAL_HEX32( TEST_DMA_PERIPH_ADDR, periphAddr );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Get_MemoryAddr( DMA_PERIPH_2, DMA_CHANNEL_5, &memoryAddr ) );
    TEST_ASSERT_EQUAL_HEX32( TEST_DMA_MEMORY_ADDR, memoryAddr );
}


/**
 * \brief   Address increments are written to CCR and read back.
 *
 * \par Expected results
 * - PINC / MINC follow requested increment, getters return it.
 * - Invalid increment values are rejected.
 */
void Ut_Dma_Set_AddrIncrement_RoundTrip( void )
{
    dma_PeriphAddrInc_t         periphInc  = DMA_PERIPH_ADDR_STATIC;
    dma_MemoryAddrInc_t         memoryInc  = DMA_MEMORY_ADDR_STATIC;
    DMA_Channel_TypeDef * const channelReg = Ut_Dma_Get_ChannelReg( 0u, TEST_DMA_LAST_CHANNEL );

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_PeriphAddrIncrement( DMA_PERIPH_1, TEST_DMA_LAST_CHANNEL, DMA_PERIPH_ADDR_INCREMENT ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_MemoryAddrIncrement( DMA_PERIPH_1, TEST_DMA_LAST_CHANNEL, DMA_MEMORY_ADDR_INCREMENT ) );
    TEST_ASSERT_EQUAL_HEX32( DMA_CCR_PINC | DMA_CCR_MINC, channelReg->CCR );

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Get_PeriphAddrIncrement( DMA_PERIPH_1, TEST_DMA_LAST_CHANNEL, &periphInc ) );
    TEST_ASSERT_EQUAL( DMA_PERIPH_ADDR_INCREMENT, periphInc );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Get_MemoryAddrIncrement( DMA_PERIPH_1, TEST_DMA_LAST_CHANNEL, &memoryInc ) );
    TEST_ASSERT_EQUAL( DMA_MEMORY_ADDR_INCREMENT, memoryInc );

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_PeriphAddrIncrement( DMA_PERIPH_1, TEST_DMA_LAST_CHANNEL, DMA_PERIPH_ADDR_STATIC ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_MemoryAddrIncrement( DMA_PERIPH_1, TEST_DMA_LAST_CHANNEL, DMA_MEMORY_ADDR_STATIC ) );
    TEST_ASSERT_EQUAL_HEX32( 0u, channelReg->CCR );

    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_PeriphAddrIncrement( DMA_PERIPH_1, TEST_DMA_LAST_CHANNEL, (dma_PeriphAddrInc_t)DMA_CCR_EN ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_MemoryAddrIncrement( DMA_PERIPH_1, TEST_DMA_LAST_CHANNEL, (dma_MemoryAddrInc_t)DMA_CCR_EN ) );
}


/**
 * \brief   Transfer sizes are written to CCR and read back.
 *
 * \details All sizes on peripheral and memory side.
 *
 * \par Expected results
 * - PSIZE / MSIZE hold byte (0), half word (1), word (2), getters return sizes.
 */
void Ut_Dma_Set_TransferSize_AllSizes_RoundTrip( void )
{
    static const uint32_t       psizeLut[ DMA_TRANSFER_SIZE_CNT ] = { 0u, DMA_CCR_PSIZE_0, DMA_CCR_PSIZE_1 };
    static const uint32_t       msizeLut[ DMA_TRANSFER_SIZE_CNT ] = { 0u, DMA_CCR_MSIZE_0, DMA_CCR_MSIZE_1 };
    dma_TransferSize_t          readSize   = DMA_TRANSFER_SIZE_8BIT;
    DMA_Channel_TypeDef * const channelReg = Ut_Dma_Get_ChannelReg( 1u, 2u );

    for( uint32_t size = 0u; DMA_TRANSFER_SIZE_CNT > size; size++ )
    {
        TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_PeriphTransferSize( DMA_PERIPH_2, DMA_CHANNEL_3, (dma_TransferSize_t)size ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_MemoryTransferSize( DMA_PERIPH_2, DMA_CHANNEL_3, (dma_TransferSize_t)size ) );
        TEST_ASSERT_EQUAL_HEX32( psizeLut[ size ] | msizeLut[ size ], channelReg->CCR );

        TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Get_PeriphTransferSize( DMA_PERIPH_2, DMA_CHANNEL_3, &readSize ) );
        TEST_ASSERT_EQUAL( size, readSize );
        TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Get_MemoryTransferSize( DMA_PERIPH_2, DMA_CHANNEL_3, &readSize ) );
        TEST_ASSERT_EQUAL( size, readSize );
    }

    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_PeriphTransferSize( DMA_PERIPH_2, DMA_CHANNEL_3, DMA_TRANSFER_SIZE_CNT ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_MemoryTransferSize( DMA_PERIPH_2, DMA_CHANNEL_3, DMA_TRANSFER_SIZE_CNT ) );
}


/**
 * \brief   Data count is written to CNDTR and read back, range is checked.
 *
 * \par Expected results
 * - 0xFFFF accepted, 0x10000 rejected (CNDTR unchanged).
 */
void Ut_Dma_Set_DataCount_RangeChecked( void )
{
    dma_DataCount_t             dataCount  = 0u;
    DMA_Channel_TypeDef * const channelReg = Ut_Dma_Get_ChannelReg( 0u, 1u );

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_DataCount( DMA_PERIPH_1, DMA_CHANNEL_2, DMA_DATA_COUNT_MAX ) );
    TEST_ASSERT_EQUAL_HEX32( DMA_DATA_COUNT_MAX, channelReg->CNDTR );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Get_DataCount( DMA_PERIPH_1, DMA_CHANNEL_2, &dataCount ) );
    TEST_ASSERT_EQUAL_UINT32( DMA_DATA_COUNT_MAX, dataCount );

    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_DataCount( DMA_PERIPH_1, DMA_CHANNEL_2, DMA_DATA_COUNT_MAX + 1u ) );
    TEST_ASSERT_EQUAL_HEX32( DMA_DATA_COUNT_MAX, channelReg->CNDTR );
}


/**
 * \brief   Peripheral request is written to request register of the DMA channel and read back.
 *
 * \details DMA1 channel 3, SPI1 TX request (served by DMA1 channel 3 on STM32L4).
 *          Other bits of the register are preset (DMAMUX1: SOIE, DMA_CSELR:
 *          fields of other channels). Request identification above the last
 *          request of the device.
 *
 * \par Expected results
 * - STM32L4+: DMAREQ_ID of DMAMUX1 channel 2 holds SPI1 TX request, SOIE kept.
 * - STM32L4: C3S field of DMA1_CSELR holds 1 (SPI1 TX), other fields kept.
 * - Getter returns DMA_REQ_SPI1_TX.
 * - Invalid identification rejected, register unchanged.
 */
void Ut_Dma_Set_PeripheralRequest_RoundTrip( void )
{
    dma_PeriphReqId_t requestId = DMA_REQ_MEM2MEM;

#if defined(DMAMUX1)
    volatile uint32_t * const requestReg  = &DMAMUX1_Channel2->CCR;
    const uint32_t            otherBits   = DMAMUX_CxCR_SOIE;
    const uint32_t            expectedReg = otherBits | TEST_DMA_SPI1_TX_REG;
#else
    volatile uint32_t * const requestReg  = &DMA1_CSELR->CSELR;
    const uint32_t            otherBits   = 0x0FFFF0FFu;  /* All fields of channels 1, 2, 4 - 7 */
    const uint32_t            expectedReg = otherBits | ( TEST_DMA_SPI1_TX_REG << DMA_CSELR_C3S_Pos );
#endif

    *requestReg = otherBits;

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_PeripheralRequest( DMA_PERIPH_1, DMA_CHANNEL_3, DMA_REQ_SPI1_TX ) );
    TEST_ASSERT_EQUAL_HEX32( expectedReg, *requestReg );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Get_PeripheralRequest( DMA_PERIPH_1, DMA_CHANNEL_3, &requestId ) );
    TEST_ASSERT_EQUAL( DMA_REQ_SPI1_TX, requestId );

    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_PeripheralRequest( DMA_PERIPH_1, DMA_CHANNEL_3, TEST_DMA_INVALID_REQUEST ) );
    TEST_ASSERT_EQUAL_HEX32( expectedReg, *requestReg );
}


/**
 * \brief   Memory to memory request selects no request line and is read back.
 *
 * \details Request register of DMA1 channel 4 holds a peripheral request,
 *          DMA_REQ_MEM2MEM is set.
 *
 * \par Expected results
 * - Request register of the channel is 0 (DMAMUX1 MEM2MEM / CSELR reset value).
 * - Getter returns DMA_REQ_MEM2MEM.
 */
void Ut_Dma_Set_PeripheralRequest_MemToMem_ClearsRequest( void )
{
    dma_PeriphReqId_t requestId = DMA_REQ_ADC1;

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_PeripheralRequest( DMA_PERIPH_1, DMA_CHANNEL_4, DMA_REQ_USART1_TX ) );
    TEST_ASSERT_NOT_EQUAL( 0u, Ut_Dma_Get_RequestReg( 0u, 3u ) );

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_PeripheralRequest( DMA_PERIPH_1, DMA_CHANNEL_4, DMA_REQ_MEM2MEM ) );
    TEST_ASSERT_EQUAL_HEX32( 0u, Ut_Dma_Get_RequestReg( 0u, 3u ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Get_PeripheralRequest( DMA_PERIPH_1, DMA_CHANNEL_4, &requestId ) );
    TEST_ASSERT_EQUAL( DMA_REQ_MEM2MEM, requestId );
}


/**
 * \brief   DMA2 channel 1 request is routed by the DMAMUX1 channel following the last DMA1 channel.
 *
 * \details Request of DMA2 channel 1 is set (STM32L4+, DMAMUX1 channel 7).
 *
 * \par Expected results
 * - DMAREQ_ID of DMAMUX1 channel TEST_DMA_CHANNELS_PER_DMA holds the request.
 * - DMAMUX1 channel 8 (DMA2 channel 2) is not written.
 */
void Ut_Dma_Set_PeripheralRequest_Dma2Channel1_DmamuxAfterDma1Channels( void )
{
#if defined(DMAMUX1)
    DMAMUX_Channel_TypeDef * const expectedMux = (DMAMUX_Channel_TypeDef *)( DMAMUX1_Channel0_BASE + ( TEST_DMA_CHANNELS_PER_DMA * TEST_DMA_MUX_REG_SIZE ) );

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_PeripheralRequest( DMA_PERIPH_2, DMA_CHANNEL_1, DMA_REQ_USART1_TX ) );

    TEST_ASSERT_EQUAL_HEX32( LL_DMAMUX_REQ_USART1_TX, expectedMux->CCR );
    TEST_ASSERT_EQUAL_HEX32( 0u, ( (DMAMUX_Channel_TypeDef *)( DMAMUX1_Channel0_BASE + ( 8u * TEST_DMA_MUX_REG_SIZE ) ) )->CCR );
#else
    TEST_IGNORE_MESSAGE( "MCU without DMAMUX1" );
#endif /* DMAMUX1 */
}


/**
 * \brief   Every DMA channel has its own DMAMUX1 channel (STM32L4+).
 *
 * \details Request ( channel number + 1 ) is set for every channel of DMA1 and DMA2.
 *
 * \par Expected results
 * - DMAMUX1 channel ( DMA index x channels per DMA + channel index ) holds the
 *   request of the channel, getter returns it.
 */
void Ut_Dma_Set_PeripheralRequest_EveryChannel_OwnDmamuxChannel( void )
{
#if defined(DMAMUX1)
    dma_PeriphReqId_t requestId = DMA_REQ_MEM2MEM;

    for( uint32_t dmaIdx = 0u; DMA_PERIPH_CNT > dmaIdx; dmaIdx++ )
    {
        for( uint32_t channelIdx = 0u; DMA_CHANNEL_CNT > channelIdx; channelIdx++ )
        {
            const uint32_t reqValue = ( dmaIdx * TEST_DMA_CHANNELS_PER_DMA ) + channelIdx + 1u;

            TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_PeripheralRequest( (dma_PeriphId_t)dmaIdx, (dma_ChannelId_t)channelIdx, (dma_PeriphReqId_t)reqValue ) );
        }
    }

    for( uint32_t dmaIdx = 0u; DMA_PERIPH_CNT > dmaIdx; dmaIdx++ )
    {
        for( uint32_t channelIdx = 0u; DMA_CHANNEL_CNT > channelIdx; channelIdx++ )
        {
            const uint32_t reqValue = ( dmaIdx * TEST_DMA_CHANNELS_PER_DMA ) + channelIdx + 1u;

            TEST_ASSERT_EQUAL_HEX32( reqValue, Ut_Dma_Get_RequestReg( dmaIdx, channelIdx ) );
            TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Get_PeripheralRequest( (dma_PeriphId_t)dmaIdx, (dma_ChannelId_t)channelIdx, &requestId ) );
            TEST_ASSERT_EQUAL( reqValue, requestId );
        }
    }
#else
    TEST_IGNORE_MESSAGE( "MCU without DMAMUX1" );
#endif /* DMAMUX1 */
}


/**
 * \brief   Request not served by the channel is refused (STM32L4 fixed mapping).
 *
 * \details USART2 TX is served by DMA1 channel 7 only, USART1 TX by DMA1
 *          channel 4 and DMA2 channel 6 (RM0394 DMA1 / DMA2 request tables).
 *          C3S field of DMA1 channel 3 holds SPI1 TX request.
 *
 * \par Expected results
 * - USART2 TX on DMA1 channel 3 and USART1 TX on DMA2 channel 1 are refused,
 *   DMA1_CSELR / DMA2_CSELR unchanged.
 * - USART2 TX on DMA1 channel 7 selects C7S = 2, USART1 TX on DMA2 channel 6
 *   selects C6S = 2.
 */
void Ut_Dma_Set_PeripheralRequest_NotMappedToChannel_Refused( void )
{
#if !defined(DMAMUX1)
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_PeripheralRequest( DMA_PERIPH_1, DMA_CHANNEL_3, DMA_REQ_SPI1_TX ) );

    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_PeripheralRequest( DMA_PERIPH_1, DMA_CHANNEL_3, DMA_REQ_USART2_TX ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_PeripheralRequest( DMA_PERIPH_2, DMA_CHANNEL_1, DMA_REQ_USART1_TX ) );
    TEST_ASSERT_EQUAL_HEX32( TEST_DMA_SPI1_TX_REG << DMA_CSELR_C3S_Pos, DMA1_CSELR->CSELR );
    TEST_ASSERT_EQUAL_HEX32( 0u, DMA2_CSELR->CSELR );

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_PeripheralRequest( DMA_PERIPH_1, DMA_CHANNEL_7, DMA_REQ_USART2_TX ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_PeripheralRequest( DMA_PERIPH_2, DMA_CHANNEL_6, DMA_REQ_USART1_TX ) );
    TEST_ASSERT_EQUAL_HEX32( ( TEST_DMA_SPI1_TX_REG << DMA_CSELR_C3S_Pos ) | ( 2u << DMA_CSELR_C7S_Pos ), DMA1_CSELR->CSELR );
    TEST_ASSERT_EQUAL_HEX32( 2u << DMA_CSELR_C6S_Pos, DMA2_CSELR->CSELR );
#else
    TEST_IGNORE_MESSAGE( "MCU with DMAMUX1 - no fixed request mapping" );
#endif /* DMAMUX1 */
}


/**
 * \brief   Requests sharing one request line of the channel are read back as configured.
 *
 * \details TIM1 CH4, TIM1 TRIG and TIM1 COM share C4S = 7 of DMA1 channel 4.
 *          TIM1 TRIG is configured, then C4S is changed outside of the module
 *          to the value of USART1 TX (2) and back to 7.
 *
 * \par Expected results
 * - C4S = 7, getter returns DMA_REQ_TIM1_TRIG (configured request).
 * - C4S = 2 (external change): getter returns DMA_REQ_USART1_TX.
 * - C4S = 7 again: getter returns DMA_REQ_TIM1_TRIG.
 */
void Ut_Dma_Get_PeripheralRequest_SharedRequestLine_ReturnsConfigured( void )
{
#if !defined(DMAMUX1)
    dma_PeriphReqId_t requestId = DMA_REQ_MEM2MEM;

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_PeripheralRequest( DMA_PERIPH_1, DMA_CHANNEL_4, DMA_REQ_TIM1_TRIG ) );
    TEST_ASSERT_EQUAL_HEX32( 7u, Ut_Dma_Get_RequestReg( 0u, 3u ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Get_PeripheralRequest( DMA_PERIPH_1, DMA_CHANNEL_4, &requestId ) );
    TEST_ASSERT_EQUAL( DMA_REQ_TIM1_TRIG, requestId );

    Ut_Dma_Set_RequestReg( 0u, 3u, 2u );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Get_PeripheralRequest( DMA_PERIPH_1, DMA_CHANNEL_4, &requestId ) );
    TEST_ASSERT_EQUAL( DMA_REQ_USART1_TX, requestId );

    Ut_Dma_Set_RequestReg( 0u, 3u, 7u );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Get_PeripheralRequest( DMA_PERIPH_1, DMA_CHANNEL_4, &requestId ) );
    TEST_ASSERT_EQUAL( DMA_REQ_TIM1_TRIG, requestId );
#else
    TEST_IGNORE_MESSAGE( "MCU with DMAMUX1 - no fixed request mapping" );
#endif /* DMAMUX1 */
}


/**
 * \brief   Request line value changed outside of the module returns the first mapped request.
 *
 * \details MEM2MEM is configured by the module on DMA1 channel 4 (the module
 *          keeps configured request between tests). C4S is set to 7 (TIM1
 *          CH4 / TRIG / COM) and to 0xF (no request mapped to the value).
 *
 * \par Expected results
 * - C4S = 7: getter returns DMA_REQ_TIM1_CH4 (first request of the value).
 * - C4S = 0xF: getter returns DMA_REQ_MEM2MEM.
 */
void Ut_Dma_Get_PeripheralRequest_ExternalValue_ReturnsFirstMapped( void )
{
#if !defined(DMAMUX1)
    dma_PeriphReqId_t requestId = DMA_REQ_MEM2MEM;

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_PeripheralRequest( DMA_PERIPH_1, DMA_CHANNEL_4, DMA_REQ_MEM2MEM ) );

    Ut_Dma_Set_RequestReg( 0u, 3u, 7u );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Get_PeripheralRequest( DMA_PERIPH_1, DMA_CHANNEL_4, &requestId ) );
    TEST_ASSERT_EQUAL( DMA_REQ_TIM1_CH4, requestId );

    Ut_Dma_Set_RequestReg( 0u, 3u, 0xFu );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Get_PeripheralRequest( DMA_PERIPH_1, DMA_CHANNEL_4, &requestId ) );
    TEST_ASSERT_EQUAL( DMA_REQ_MEM2MEM, requestId );
#else
    TEST_IGNORE_MESSAGE( "MCU with DMAMUX1 - no fixed request mapping" );
#endif /* DMAMUX1 */
}


/**
 * \brief   Every request of the device is served by at least one DMA channel.
 *
 * \details Every request identification (except MEM2MEM) is set on every
 *          channel of DMA1 and DMA2 until accepted. Accepted request is read
 *          back by the getter.
 *
 * \par Expected results
 * - Every request is accepted by at least one channel, getter of the channel
 *   returns the request.
 */
void Ut_Dma_Set_PeripheralRequest_EveryRequest_ServedByChannel( void )
{
#if !defined(DMAMUX1)
    dma_PeriphReqId_t requestId = DMA_REQ_MEM2MEM;

    for( uint32_t reqIdx = (uint32_t)DMA_REQ_MEM2MEM + 1u; (uint32_t)DMA_REQ_CNT > reqIdx; reqIdx++ )
    {
        uint32_t acceptedCnt = 0u;

        for( uint32_t dmaIdx = 0u; DMA_PERIPH_CNT > dmaIdx; dmaIdx++ )
        {
            for( uint32_t channelIdx = 0u; DMA_CHANNEL_CNT > channelIdx; channelIdx++ )
            {
                const dma_RequestState_t setState = Dma_Set_PeripheralRequest( (dma_PeriphId_t)dmaIdx, (dma_ChannelId_t)channelIdx, (dma_PeriphReqId_t)reqIdx );

                if( DMA_REQUEST_OK == setState )
                {
                    acceptedCnt++;

                    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Get_PeripheralRequest( (dma_PeriphId_t)dmaIdx, (dma_ChannelId_t)channelIdx, &requestId ) );
                    TEST_ASSERT_EQUAL_MESSAGE( reqIdx, requestId, "Accepted request not read back" );
                }
                else
                {
                    /* Request not served by the channel */
                }
            }
        }

        TEST_ASSERT_NOT_EQUAL_MESSAGE( 0u, acceptedCnt, "Request not served by any channel" );
    }
#else
    TEST_IGNORE_MESSAGE( "MCU with DMAMUX1 - no fixed request mapping" );
#endif /* DMAMUX1 */
}


/**
 * \brief   Channel priority is written to PL and read back.
 *
 * \par Expected results
 * - PL holds 0 - 3 for low - very high priority, getter returns priority.
 * - DMA_PRIORITY_CNT is rejected.
 */
void Ut_Dma_Set_Priority_AllLevels_RoundTrip( void )
{
    dma_Priority_t              priority   = DMA_PRIORITY_LOW;
    DMA_Channel_TypeDef * const channelReg = Ut_Dma_Get_ChannelReg( 1u, 3u );

    for( uint32_t level = 0u; DMA_PRIORITY_CNT > level; level++ )
    {
        TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_Priority( DMA_PERIPH_2, DMA_CHANNEL_4, (dma_Priority_t)level ) );
        TEST_ASSERT_EQUAL_HEX32( level << DMA_CCR_PL_Pos, channelReg->CCR );
        TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Get_Priority( DMA_PERIPH_2, DMA_CHANNEL_4, &priority ) );
        TEST_ASSERT_EQUAL( level, priority );
    }

    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_Priority( DMA_PERIPH_2, DMA_CHANNEL_4, DMA_PRIORITY_CNT ) );
}


/**
 * \brief   Configuration of enabled channel is rejected.
 *
 * \details Channel configuration registers are write protected by hardware
 *          while EN is set. DMA2 channel 6, USART1 TX request (served by DMA2
 *          channel 6 on STM32L4).
 *
 * \par Expected results
 * - All configuration setters return DMA_REQUEST_ERROR.
 * - CCR, CPAR, CMAR, CNDTR and request register (DMAMUX1 channel / C6S field
 *   of DMA2_CSELR) are not changed.
 */
void Ut_Dma_Set_Configuration_ChannelEnabled_ReturnsError( void )
{
    DMA_Channel_TypeDef * const channelReg = Ut_Dma_Get_ChannelReg( 1u, 5u );

    channelReg->CCR = DMA_CCR_EN;
    Ut_Dma_Set_RequestReg( 1u, 5u, 3u );  /* Request selected before channel enable */

    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_Direction( DMA_PERIPH_2, DMA_CHANNEL_6, DMA_DIR_MEMORY_TO_PERIPH ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_TransferMode( DMA_PERIPH_2, DMA_CHANNEL_6, DMA_TRANSFER_MODE_CIRCULAR ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_PeriphAddr( DMA_PERIPH_2, DMA_CHANNEL_6, TEST_DMA_PERIPH_ADDR ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_MemoryAddr( DMA_PERIPH_2, DMA_CHANNEL_6, TEST_DMA_MEMORY_ADDR ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_PeriphAddrIncrement( DMA_PERIPH_2, DMA_CHANNEL_6, DMA_PERIPH_ADDR_INCREMENT ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_MemoryAddrIncrement( DMA_PERIPH_2, DMA_CHANNEL_6, DMA_MEMORY_ADDR_INCREMENT ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_PeriphTransferSize( DMA_PERIPH_2, DMA_CHANNEL_6, DMA_TRANSFER_SIZE_32BIT ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_MemoryTransferSize( DMA_PERIPH_2, DMA_CHANNEL_6, DMA_TRANSFER_SIZE_32BIT ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_DataCount( DMA_PERIPH_2, DMA_CHANNEL_6, 10u ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_PeripheralRequest( DMA_PERIPH_2, DMA_CHANNEL_6, DMA_REQ_USART1_TX ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_Priority( DMA_PERIPH_2, DMA_CHANNEL_6, DMA_PRIORITY_HIGH ) );

    TEST_ASSERT_EQUAL_HEX32( DMA_CCR_EN, channelReg->CCR );
    TEST_ASSERT_EQUAL_HEX32( 0u, channelReg->CPAR );
    TEST_ASSERT_EQUAL_HEX32( 0u, channelReg->CMAR );
    TEST_ASSERT_EQUAL_HEX32( 0u, channelReg->CNDTR );
    TEST_ASSERT_EQUAL_HEX32( 3u, Ut_Dma_Get_RequestReg( 1u, 5u ) );
}


/**
 * \brief   All functions reject invalid DMA peripheral and channel.
 *
 * \par Expected results
 * - DMA_REQUEST_ERROR for DMA_PERIPH_CNT and DMA_CHANNEL_CNT, registers of all
 *   channels and DMAMUX1 channels are not changed.
 */
void Ut_Dma_InvalidIdentifiers_ReturnError( void )
{
    static const dma_PeriphId_t  busLut[]     = { DMA_PERIPH_CNT, DMA_PERIPH_1 };
    static const dma_ChannelId_t channelLut[] = { DMA_CHANNEL_1, DMA_CHANNEL_CNT };
    dma_FunctionState_t state       = DMA_FUNCTION_INACTIVE;
    dma_Direction_t     direction   = DMA_DIR_PERIPH_TO_MEMORY;
    dma_TransferMode_t  mode        = DMA_TRANSFER_MODE_NORMAL;
    dma_PeriphAddr_t    periphAddr  = 0u;
    dma_MemoryAddr_t    memoryAddr  = 0u;
    dma_PeriphAddrInc_t periphInc   = DMA_PERIPH_ADDR_STATIC;
    dma_MemoryAddrInc_t memoryInc   = DMA_MEMORY_ADDR_STATIC;
    dma_TransferSize_t  size        = DMA_TRANSFER_SIZE_8BIT;
    dma_DataCount_t     dataCount   = 0u;
    dma_PeriphReqId_t   requestId   = DMA_REQ_MEM2MEM;
    dma_Priority_t      priority    = DMA_PRIORITY_LOW;
    dma_IsrCallback     callback    = NULL;

    for( uint32_t idx = 0u; 2u > idx; idx++ )
    {
        dma_PeriphId_t  bus     = busLut[ idx ];
        dma_ChannelId_t channel = channelLut[ idx ];

        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_TransferActive( bus, channel ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_TransferInactive( bus, channel ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Get_TransferState( bus, channel, &state ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_ChannelActive( bus, channel ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_ChannelInactive( bus, channel ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Get_ChannelState( bus, channel, &state ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_Direction( bus, channel, DMA_DIR_MEMORY_TO_PERIPH ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Get_Direction( bus, channel, &direction ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_TransferMode( bus, channel, DMA_TRANSFER_MODE_CIRCULAR ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Get_TransferMode( bus, channel, &mode ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_PeriphAddr( bus, channel, TEST_DMA_PERIPH_ADDR ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Get_PeriphAddr( bus, channel, &periphAddr ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_MemoryAddr( bus, channel, TEST_DMA_MEMORY_ADDR ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Get_MemoryAddr( bus, channel, &memoryAddr ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_PeriphAddrIncrement( bus, channel, DMA_PERIPH_ADDR_INCREMENT ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Get_PeriphAddrIncrement( bus, channel, &periphInc ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_MemoryAddrIncrement( bus, channel, DMA_MEMORY_ADDR_INCREMENT ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Get_MemoryAddrIncrement( bus, channel, &memoryInc ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_PeriphTransferSize( bus, channel, DMA_TRANSFER_SIZE_16BIT ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Get_PeriphTransferSize( bus, channel, &size ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_MemoryTransferSize( bus, channel, DMA_TRANSFER_SIZE_16BIT ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Get_MemoryTransferSize( bus, channel, &size ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_DataCount( bus, channel, 1u ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Get_DataCount( bus, channel, &dataCount ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_PeripheralRequest( bus, channel, DMA_REQ_USART1_TX ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Get_PeripheralRequest( bus, channel, &requestId ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_Priority( bus, channel, DMA_PRIORITY_HIGH ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Get_Priority( bus, channel, &priority ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_InterruptActive( bus, channel ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_InterruptInactive( bus, channel ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_TransferCompleteIrqActive( bus, channel ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_TransferCompleteIrqInactive( bus, channel ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_HalfTransferIrqActive( bus, channel ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_HalfTransferIrqInactive( bus, channel ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_TransferErrorIrqActive( bus, channel ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_TransferErrorIrqInactive( bus, channel ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_TransferCompleteIsrHandler( bus, channel, Ut_Dma_TransferCompleteCallback ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Get_TransferCompleteIsrHandler( bus, channel, &callback ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_HalfTransferIsrHandler( bus, channel, Ut_Dma_HalfTransferCallback ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Get_HalfTransferIsrHandler( bus, channel, &callback ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_TransferErrorIsrHandler( bus, channel, Ut_Dma_TransferErrorCallback ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Get_TransferErrorIsrHandler( bus, channel, &callback ) );
    }

    for( uint32_t dmaIdx = 0u; DMA_PERIPH_CNT > dmaIdx; dmaIdx++ )
    {
        for( uint32_t channelIdx = 0u; DMA_CHANNEL_CNT > channelIdx; channelIdx++ )
        {
            TEST_ASSERT_EQUAL_HEX32( 0u, Ut_Dma_Get_ChannelReg( dmaIdx, channelIdx )->CCR );
            TEST_ASSERT_EQUAL_HEX32( 0u, Ut_Dma_Get_ChannelReg( dmaIdx, channelIdx )->CPAR );
            TEST_ASSERT_EQUAL_HEX32( 0u, Ut_Dma_Get_ChannelReg( dmaIdx, channelIdx )->CMAR );
            TEST_ASSERT_EQUAL_HEX32( 0u, Ut_Dma_Get_ChannelReg( dmaIdx, channelIdx )->CNDTR );
            TEST_ASSERT_EQUAL_HEX32( 0u, Ut_Dma_Get_RequestReg( dmaIdx, channelIdx ) );
        }
    }
}


/**
 * \brief   Getters reject null pointer.
 *
 * \par Expected results
 * - DMA_REQUEST_ERROR is returned.
 */
void Ut_Dma_Getters_NullPointer_ReturnError( void )
{
    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Get_TransferState( DMA_PERIPH_1, DMA_CHANNEL_1, NULL ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Get_ChannelState( DMA_PERIPH_1, DMA_CHANNEL_1, NULL ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Get_Direction( DMA_PERIPH_1, DMA_CHANNEL_1, NULL ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Get_TransferMode( DMA_PERIPH_1, DMA_CHANNEL_1, NULL ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Get_PeriphAddr( DMA_PERIPH_1, DMA_CHANNEL_1, NULL ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Get_MemoryAddr( DMA_PERIPH_1, DMA_CHANNEL_1, NULL ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Get_PeriphAddrIncrement( DMA_PERIPH_1, DMA_CHANNEL_1, NULL ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Get_MemoryAddrIncrement( DMA_PERIPH_1, DMA_CHANNEL_1, NULL ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Get_PeriphTransferSize( DMA_PERIPH_1, DMA_CHANNEL_1, NULL ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Get_MemoryTransferSize( DMA_PERIPH_1, DMA_CHANNEL_1, NULL ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Get_DataCount( DMA_PERIPH_1, DMA_CHANNEL_1, NULL ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Get_PeripheralRequest( DMA_PERIPH_1, DMA_CHANNEL_1, NULL ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Get_Priority( DMA_PERIPH_1, DMA_CHANNEL_1, NULL ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Get_TransferCompleteIsrHandler( DMA_PERIPH_1, DMA_CHANNEL_1, NULL ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Get_HalfTransferIsrHandler( DMA_PERIPH_1, DMA_CHANNEL_1, NULL ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Get_TransferErrorIsrHandler( DMA_PERIPH_1, DMA_CHANNEL_1, NULL ) );
}

/* ============================== INTERRUPTS ================================ */

/**
 * \brief   Dma_Set_InterruptActive() registers channel handler and enables IRQ.
 *
 * \details Last channel of DMA2 (non contiguous NVIC number).
 *
 * \par Expected results
 * - Handler and activation of the NVIC interrupt of the channel, DMA_REQUEST_OK.
 */
void Ut_Dma_Set_InterruptActive_RegistersHandlerAndEnablesIrq( void )
{
    TEST_ASSERT_NOT_NULL( Ut_Dma_Capture_IsrHandler( DMA_PERIPH_2, TEST_DMA_LAST_CHANNEL, utDma_IrqLut[ 1u ][ TEST_DMA_LAST_CHANNEL ] ) );
}


/**
 * \brief   Dma_Set_InterruptActive() reports NVIC error.
 *
 * \par Expected results
 * - DMA_REQUEST_ERROR is returned.
 */
void Ut_Dma_Set_InterruptActive_NvicError_ReturnsError( void )
{
    Nvic_Set_PeriphIrq_Handler_ExpectAndReturn( NVIC_PERIPH_IRQ_DMA1_CHANNEL6, NULL, NVIC_REQUEST_OK );
    Nvic_Set_PeriphIrq_Handler_IgnoreArg_irqHandler();
    Nvic_Set_PeriphIrq_Active_ExpectAndReturn( NVIC_PERIPH_IRQ_DMA1_CHANNEL6, NVIC_REQUEST_ERROR );

    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_InterruptActive( DMA_PERIPH_1, DMA_CHANNEL_6 ) );
}


/**
 * \brief   Dma_Set_InterruptInactive() disables channel IRQ.
 *
 * \par Expected results
 * - Nvic_Set_PeriphIrq_Inactive( NVIC_PERIPH_IRQ_DMA1_CHANNEL1 ), DMA_REQUEST_OK.
 * - NVIC error is reported as DMA_REQUEST_ERROR.
 */
void Ut_Dma_Set_InterruptInactive_DisablesIrq( void )
{
    Nvic_Set_PeriphIrq_Inactive_ExpectAndReturn( NVIC_PERIPH_IRQ_DMA1_CHANNEL1, NVIC_REQUEST_OK );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_InterruptInactive( DMA_PERIPH_1, DMA_CHANNEL_1 ) );

    Nvic_Set_PeriphIrq_Inactive_ExpectAndReturn( NVIC_PERIPH_IRQ_DMA1_CHANNEL1, NVIC_REQUEST_ERROR );
    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_InterruptInactive( DMA_PERIPH_1, DMA_CHANNEL_1 ) );
}


/**
 * \brief   Interrupt enable functions control TCIE, HTIE and TEIE.
 *
 * \par Expected results
 * - Each function sets or clears its own bit in CCR only.
 */
void Ut_Dma_Set_IrqEnable_ControlsInterruptBits( void )
{
    DMA_Channel_TypeDef * const channelReg = Ut_Dma_Get_ChannelReg( 1u, 5u );

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_TransferCompleteIrqActive( DMA_PERIPH_2, DMA_CHANNEL_6 ) );
    TEST_ASSERT_EQUAL_HEX32( DMA_CCR_TCIE, channelReg->CCR );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_HalfTransferIrqActive( DMA_PERIPH_2, DMA_CHANNEL_6 ) );
    TEST_ASSERT_EQUAL_HEX32( DMA_CCR_TCIE | DMA_CCR_HTIE, channelReg->CCR );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_TransferErrorIrqActive( DMA_PERIPH_2, DMA_CHANNEL_6 ) );
    TEST_ASSERT_EQUAL_HEX32( DMA_CCR_TCIE | DMA_CCR_HTIE | DMA_CCR_TEIE, channelReg->CCR );

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_TransferCompleteIrqInactive( DMA_PERIPH_2, DMA_CHANNEL_6 ) );
    TEST_ASSERT_EQUAL_HEX32( DMA_CCR_HTIE | DMA_CCR_TEIE, channelReg->CCR );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_HalfTransferIrqInactive( DMA_PERIPH_2, DMA_CHANNEL_6 ) );
    TEST_ASSERT_EQUAL_HEX32( DMA_CCR_TEIE, channelReg->CCR );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_TransferErrorIrqInactive( DMA_PERIPH_2, DMA_CHANNEL_6 ) );
    TEST_ASSERT_EQUAL_HEX32( 0u, channelReg->CCR );
}


/**
 * \brief   Callbacks are stored and returned.
 *
 * \par Expected results
 * - Getters return callbacks set by setters, null pointer clears callback.
 */
void Ut_Dma_Set_IsrHandlers_RoundTrip( void )
{
    dma_IsrCallback callback = NULL;

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_TransferCompleteIsrHandler( DMA_PERIPH_1, DMA_CHANNEL_5, Ut_Dma_TransferCompleteCallback ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_HalfTransferIsrHandler( DMA_PERIPH_1, DMA_CHANNEL_5, Ut_Dma_HalfTransferCallback ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_TransferErrorIsrHandler( DMA_PERIPH_1, DMA_CHANNEL_5, Ut_Dma_TransferErrorCallback ) );

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Get_TransferCompleteIsrHandler( DMA_PERIPH_1, DMA_CHANNEL_5, &callback ) );
    TEST_ASSERT_EQUAL_PTR( Ut_Dma_TransferCompleteCallback, callback );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Get_HalfTransferIsrHandler( DMA_PERIPH_1, DMA_CHANNEL_5, &callback ) );
    TEST_ASSERT_EQUAL_PTR( Ut_Dma_HalfTransferCallback, callback );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Get_TransferErrorIsrHandler( DMA_PERIPH_1, DMA_CHANNEL_5, &callback ) );
    TEST_ASSERT_EQUAL_PTR( Ut_Dma_TransferErrorCallback, callback );

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_TransferCompleteIsrHandler( DMA_PERIPH_1, DMA_CHANNEL_5, NULL ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Get_TransferCompleteIsrHandler( DMA_PERIPH_1, DMA_CHANNEL_5, &callback ) );
    TEST_ASSERT_NULL( callback );
}


/**
 * \brief   Channel interrupt handler executes half transfer before transfer
 *          complete callback.
 *
 * \details DMA1 channel 2 (flag offset 4), HTIE and TCIE enabled, GIF2, HTIF2 and
 *          TCIF2 active.
 *
 * \par Expected results
 * - Half transfer callback executed first, then transfer complete callback.
 * - Transfer error callback is not executed.
 * - Last write to IFCR clears TCIF2 only.
 */
void Ut_Dma_Isr_HalfAndComplete_CallbacksInOrder( void )
{
    nvic_IsrCallback_t isrHandler = Ut_Dma_Capture_IsrHandler( DMA_PERIPH_1, DMA_CHANNEL_2, NVIC_PERIPH_IRQ_DMA1_CHANNEL2 );

    (void)Dma_Set_TransferCompleteIsrHandler( DMA_PERIPH_1, DMA_CHANNEL_2, Ut_Dma_TransferCompleteCallback );
    (void)Dma_Set_HalfTransferIsrHandler( DMA_PERIPH_1, DMA_CHANNEL_2, Ut_Dma_HalfTransferCallback );
    (void)Dma_Set_TransferErrorIsrHandler( DMA_PERIPH_1, DMA_CHANNEL_2, Ut_Dma_TransferErrorCallback );

    Ut_Dma_Get_ChannelReg( 0u, 1u )->CCR = DMA_CCR_EN | DMA_CCR_HTIE | DMA_CCR_TCIE;
    DMA1->ISR = DMA_ISR_GIF2 | DMA_ISR_HTIF2 | DMA_ISR_TCIF2;

    isrHandler();

    TEST_ASSERT_EQUAL_UINT32( 1u, utDma_HalfTransferSeq );
    TEST_ASSERT_EQUAL_UINT32( 2u, utDma_TransferCompleteSeq );
    TEST_ASSERT_EQUAL_UINT32( 0u, utDma_TransferErrorSeq );
    TEST_ASSERT_EQUAL_HEX32( DMA_IFCR_CTCIF2, DMA1->IFCR );
}


/**
 * \brief   Channel interrupt handler never clears the global flag of the active channel.
 *
 * \details DMA1 channel 3 enabled with TCIE, GIF3 and TCIF3 active, then TEIE with
 *          GIF3 and TEIF3 active.
 *
 * \note    Device errata (ES0430 / ES0431 / ES0523 2.3.1 "DMA disable failure and error
 *          flag omission upon simultaneous transfer error and global flag clear"), handled
 *          by AB#697: clearing GIFx while the channel is active in the same cycle as a
 *          transfer error prevents the automatic channel disable and TEIFx. Workaround - the
 *          handler clears the specific flags (CTCIFx, CHTIFx, CTEIFx) only.
 *
 * \par Expected results
 * - IFCR written with CTCIF3, then CTEIF3 - CGIF3 never written.
 */
void Ut_Dma_Isr_ActiveChannel_GlobalFlagNotCleared( void )
{
    nvic_IsrCallback_t          isrHandler = Ut_Dma_Capture_IsrHandler( DMA_PERIPH_1, DMA_CHANNEL_3, NVIC_PERIPH_IRQ_DMA1_CHANNEL3 );
    DMA_Channel_TypeDef * const channelReg = Ut_Dma_Get_ChannelReg( 0u, 2u );

    channelReg->CCR = DMA_CCR_EN | DMA_CCR_TCIE;
    DMA1->ISR       = DMA_ISR_GIF3 | DMA_ISR_TCIF3;
    isrHandler();
    TEST_ASSERT_EQUAL_HEX32( DMA_IFCR_CTCIF3, DMA1->IFCR );

    channelReg->CCR = DMA_CCR_EN | DMA_CCR_TEIE;
    DMA1->ISR       = DMA_ISR_GIF3 | DMA_ISR_TEIF3;
    isrHandler();
    TEST_ASSERT_EQUAL_HEX32( DMA_IFCR_CTEIF3, DMA1->IFCR );
}


/**
 * \brief   Channel interrupt handler ignores flags of disabled interrupts.
 *
 * \details All flags of DMA1 channel 2 are active, no interrupt is enabled.
 *
 * \par Expected results
 * - No callback executed, no flag cleared.
 */
void Ut_Dma_Isr_InterruptsDisabled_NoCallback( void )
{
    nvic_IsrCallback_t isrHandler = Ut_Dma_Capture_IsrHandler( DMA_PERIPH_1, DMA_CHANNEL_2, NVIC_PERIPH_IRQ_DMA1_CHANNEL2 );

    (void)Dma_Set_TransferCompleteIsrHandler( DMA_PERIPH_1, DMA_CHANNEL_2, Ut_Dma_TransferCompleteCallback );
    (void)Dma_Set_HalfTransferIsrHandler( DMA_PERIPH_1, DMA_CHANNEL_2, Ut_Dma_HalfTransferCallback );
    (void)Dma_Set_TransferErrorIsrHandler( DMA_PERIPH_1, DMA_CHANNEL_2, Ut_Dma_TransferErrorCallback );

    DMA1->ISR = DMA_ISR_GIF2 | DMA_ISR_HTIF2 | DMA_ISR_TCIF2 | DMA_ISR_TEIF2;

    isrHandler();

    TEST_ASSERT_EQUAL_UINT32( 0u, utDma_CallbackCnt );
    TEST_ASSERT_EQUAL_HEX32( 0u, DMA1->IFCR );
}


/**
 * \brief   Channel interrupt handler processes transfer error of its own channel.
 *
 * \details DMA2 channel 6 (flag offset 20), TEIE and TCIE enabled, TEIF6 active.
 *          Flags of other channels are active too.
 *
 * \par Expected results
 * - Transfer error callback executed only.
 * - IFCR clears TEIF6 only.
 */
void Ut_Dma_Isr_TransferError_CallbackAndClear( void )
{
    nvic_IsrCallback_t isrHandler = Ut_Dma_Capture_IsrHandler( DMA_PERIPH_2, DMA_CHANNEL_6, NVIC_PERIPH_IRQ_DMA2_CHANNEL6 );

    (void)Dma_Set_TransferCompleteIsrHandler( DMA_PERIPH_2, DMA_CHANNEL_6, Ut_Dma_TransferCompleteCallback );
    (void)Dma_Set_TransferErrorIsrHandler( DMA_PERIPH_2, DMA_CHANNEL_6, Ut_Dma_TransferErrorCallback );

    Ut_Dma_Get_ChannelReg( 1u, 5u )->CCR = DMA_CCR_TEIE | DMA_CCR_TCIE;
    DMA2->ISR = DMA_ISR_GIF6 | DMA_ISR_TEIF6 | DMA_ISR_TCIF5 | DMA_ISR_TCIF1;

    isrHandler();

    TEST_ASSERT_EQUAL_UINT32( 1u, utDma_TransferErrorSeq );
    TEST_ASSERT_EQUAL_UINT32( 0u, utDma_TransferCompleteSeq );
    TEST_ASSERT_EQUAL_HEX32( DMA_IFCR_CTEIF6, DMA2->IFCR );
}


/**
 * \brief   Channel interrupt handler clears flag without configured callback.
 *
 * \details DMA1 channel 1, TCIE enabled, TCIF1 active, callback is null.
 *
 * \par Expected results
 * - No callback executed, TCIF1 cleared.
 */
void Ut_Dma_Isr_NoCallback_ClearsFlag( void )
{
    nvic_IsrCallback_t isrHandler = Ut_Dma_Capture_IsrHandler( DMA_PERIPH_1, DMA_CHANNEL_1, NVIC_PERIPH_IRQ_DMA1_CHANNEL1 );

    (void)Dma_Set_TransferCompleteIsrHandler( DMA_PERIPH_1, DMA_CHANNEL_1, NULL );

    Ut_Dma_Get_ChannelReg( 0u, 0u )->CCR = DMA_CCR_TCIE;
    DMA1->ISR = DMA_ISR_TCIF1;

    isrHandler();

    TEST_ASSERT_EQUAL_UINT32( 0u, utDma_CallbackCnt );
    TEST_ASSERT_EQUAL_HEX32( DMA_IFCR_CTCIF1, DMA1->IFCR );
}


/**
 * \brief   Interrupt handler of every channel processes the flags of its own channel.
 *
 * \details For every channel of DMA1 and DMA2: interrupt handler registered by
 *          Dma_Set_InterruptActive() is captured, TCIE and transfer complete callback of the
 *          channel are set, TCIF of the channel is active, the handler is called.
 *
 * \par Expected results
 * - Transfer complete callback executed once per channel.
 * - CTCIF of the channel written to IFCR of the DMA peripheral.
 */
void Ut_Dma_Isr_EveryChannel_OwnFlagsProcessed( void )
{
    for( uint32_t dmaIdx = 0u; DMA_PERIPH_CNT > dmaIdx; dmaIdx++ )
    {
        for( uint32_t channelIdx = 0u; DMA_CHANNEL_CNT > channelIdx; channelIdx++ )
        {
            const dma_PeriphId_t  dmaBus     = (dma_PeriphId_t)dmaIdx;
            const dma_ChannelId_t dmaChannel = (dma_ChannelId_t)channelIdx;
            DMA_TypeDef * const   dmaReg     = Ut_Dma_Get_DmaReg( dmaIdx );
            const uint32_t        tcFlag     = DMA_ISR_TCIF1 << ( channelIdx * TEST_DMA_FLAG_SHIFT );

            const nvic_IsrCallback_t isrHandler = Ut_Dma_Capture_IsrHandler( dmaBus, dmaChannel, utDma_IrqLut[ dmaIdx ][ channelIdx ] );

            TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_TransferCompleteIsrHandler( dmaBus, dmaChannel, Ut_Dma_TransferCompleteCallback ) );

            utDma_CallbackCnt         = 0u;
            utDma_TransferCompleteSeq = 0u;
            dmaReg->ISR  = tcFlag;
            dmaReg->IFCR = 0u;
            Ut_Dma_Get_ChannelReg( dmaIdx, channelIdx )->CCR = DMA_CCR_TCIE;

            isrHandler();

            TEST_ASSERT_EQUAL_UINT32_MESSAGE( 1u, utDma_TransferCompleteSeq, "Transfer complete callback of the channel" );
            TEST_ASSERT_EQUAL_HEX32( tcFlag, dmaReg->IFCR );
        }
    }
}


/**
 * \brief   Interrupt handler of every channel ends with DSB.
 *
 * \details For every channel of DMA1 and DMA2: interrupt handler registered by
 *          Dma_Set_InterruptActive() is captured and called once without flags.
 *
 * \note    Device errata bug AB#1104 (ES0430 / ES0431 / ES0523 2.1.3 "Store immediate
 *          overlapping exception return operation might vector to incorrect interrupt", Arm ID
 *          838869): a buffered store with immediate offset still pending at the exception return
 *          may vector to an incorrect interrupt. Workaround - DSB before the exception return of
 *          every handler.
 *
 * \par Expected results
 * - Every handler executes exactly one DSB.
 */
void Ut_Dma_Isr_EveryChannel_EndsWithDsb( void )
{
    for( uint32_t dmaIdx = 0u; DMA_PERIPH_CNT > dmaIdx; dmaIdx++ )
    {
        for( uint32_t channelIdx = 0u; DMA_CHANNEL_CNT > channelIdx; channelIdx++ )
        {
            const nvic_IsrCallback_t isrHandler = Ut_Dma_Capture_IsrHandler( (dma_PeriphId_t)dmaIdx, (dma_ChannelId_t)channelIdx, utDma_IrqLut[ dmaIdx ][ channelIdx ] );

            Ut_Dma_Get_DmaReg( dmaIdx )->ISR = 0u;

            const uint32_t dsbCnt = CmsisHost_Get_InstrCnt( CMSISHOST_INSTR_DSB );

            isrHandler();

            TEST_ASSERT_EQUAL_UINT32( dsbCnt + 1u, CmsisHost_Get_InstrCnt( CMSISHOST_INSTR_DSB ) );
        }
    }
}

/* ========================== LOCAL FUNCTIONS =============================== */

/**
 * \brief Returns registers of DMA channel (calculated independently of the module).
 *
 * \param dmaIdx     [in]: DMA peripheral index (0 - DMA1, 1 - DMA2)
 * \param channelIdx [in]: Channel index (0 - channel 1)
 * \return Channel registers
 */
static DMA_Channel_TypeDef * Ut_Dma_Get_ChannelReg( uint32_t dmaIdx, uint32_t channelIdx )
{
    uint32_t channel1Base = DMA2_Channel1_BASE;

    if( 0u == dmaIdx )
    {
        channel1Base = DMA1_Channel1_BASE;
    }
    else
    {
        channel1Base = DMA2_Channel1_BASE;
    }

    return ( (DMA_Channel_TypeDef *)( channel1Base + ( channelIdx * TEST_DMA_CHANNEL_REG_SIZE ) ) );
}


#if defined(DMAMUX1)
/**
 * \brief Returns DMAMUX1 channel registers serving the DMA channel (RM0432 -
 *        DMA2 channels follow the last DMA1 channel).
 *
 * \param dmaIdx     [in]: DMA peripheral index (0 - DMA1, 1 - DMA2)
 * \param channelIdx [in]: Channel index (0 - channel 1)
 * \return DMAMUX1 channel registers
 */
static DMAMUX_Channel_TypeDef * Ut_Dma_Get_MuxReg( uint32_t dmaIdx, uint32_t channelIdx )
{
    const uint32_t muxIdx = ( dmaIdx * TEST_DMA_CHANNELS_PER_DMA ) + channelIdx;

    return ( (DMAMUX_Channel_TypeDef *)( DMAMUX1_Channel0_BASE + ( muxIdx * TEST_DMA_MUX_REG_SIZE ) ) );
}
#else
/**
 * \brief Returns channel selection register (DMA_CSELR) of DMA peripheral.
 *
 * \param dmaIdx [in]: DMA peripheral index (0 - DMA1, 1 - DMA2)
 * \return Channel selection register
 */
static DMA_Request_TypeDef * Ut_Dma_Get_CselrReg( uint32_t dmaIdx )
{
    DMA_Request_TypeDef * cselrReg = DMA1_CSELR;

    if( 0u != dmaIdx )
    {
        cselrReg = DMA2_CSELR;
    }
    else
    {
        /* No action required */
    }

    return ( cselrReg );
}
#endif /* DMAMUX1 */


/**
 * \brief Returns request selection of the DMA channel - DMAMUX1 channel CCR
 *        (STM32L4+) or CxS field of DMA_CSELR (STM32L4).
 *
 * \param dmaIdx     [in]: DMA peripheral index (0 - DMA1, 1 - DMA2)
 * \param channelIdx [in]: Channel index (0 - channel 1)
 * \return Register (field) value
 */
static uint32_t Ut_Dma_Get_RequestReg( uint32_t dmaIdx, uint32_t channelIdx )
{
#if defined(DMAMUX1)
    return ( Ut_Dma_Get_MuxReg( dmaIdx, channelIdx )->CCR );
#else
    const uint32_t fieldPos = channelIdx * TEST_DMA_CSELR_FIELD_WIDTH;

    return ( ( Ut_Dma_Get_CselrReg( dmaIdx )->CSELR >> fieldPos ) & TEST_DMA_CSELR_FIELD_MASK );
#endif
}


/**
 * \brief Writes request selection of the DMA channel (outside of the module) -
 *        DMAMUX1 channel CCR (STM32L4+) or CxS field of DMA_CSELR (STM32L4,
 *        fields of other channels kept).
 *
 * \param dmaIdx     [in]: DMA peripheral index (0 - DMA1, 1 - DMA2)
 * \param channelIdx [in]: Channel index (0 - channel 1)
 * \param regValue   [in]: Register (field) value
 */
static void Ut_Dma_Set_RequestReg( uint32_t dmaIdx, uint32_t channelIdx, uint32_t regValue )
{
#if defined(DMAMUX1)
    Ut_Dma_Get_MuxReg( dmaIdx, channelIdx )->CCR = regValue;
#else
    const uint32_t fieldPos = channelIdx * TEST_DMA_CSELR_FIELD_WIDTH;

    MODIFY_REG( Ut_Dma_Get_CselrReg( dmaIdx )->CSELR, TEST_DMA_CSELR_FIELD_MASK << fieldPos, regValue << fieldPos );
#endif
}


/**
 * \brief Returns DMA peripheral registers.
 *
 * \param dmaIdx [in]: DMA peripheral index (0 - DMA1, 1 - DMA2)
 * \return DMA peripheral registers
 */
static DMA_TypeDef * Ut_Dma_Get_DmaReg( uint32_t dmaIdx )
{
    DMA_TypeDef * dmaReg = DMA2;

    if( 0u == dmaIdx )
    {
        dmaReg = DMA1;
    }
    else
    {
        dmaReg = DMA2;
    }

    return ( dmaReg );
}


/**
 * \brief Expects read of clock state returning given state.
 *
 * \param periphId   [in]: RCC peripheral (DMA or DMAMUX1)
 * \param clockState [in]: Reported clock state
 */
static void Ut_Dma_Expect_ClockState( rcc_PeriphId_t periphId, rcc_FunctionState_t clockState )
{
    static rcc_FunctionState_t reportedState[ 2u ] = { RCC_FUNCTION_INACTIVE, RCC_FUNCTION_INACTIVE };
    static uint32_t            stateIdx            = 0u;

    /* Separate storage for consecutive expectations (DMA and DMAMUX1 clock) */
    stateIdx                  = ( stateIdx + 1u ) % 2u;
    reportedState[ stateIdx ] = clockState;

    Rcc_Get_PeriphState_ExpectAndReturn( periphId, NULL, RCC_REQUEST_OK );
    Rcc_Get_PeriphState_IgnoreArg_funcState();
    Rcc_Get_PeriphState_ReturnThruPtr_funcState( &reportedState[ stateIdx ] );
}


/**
 * \brief Expects activation of inactive clock.
 *
 * \param periphId [in]: RCC peripheral (DMA or DMAMUX1)
 */
static void Ut_Dma_Expect_ClockActivation( rcc_PeriphId_t periphId )
{
    Ut_Dma_Expect_ClockState( periphId, RCC_FUNCTION_INACTIVE );
    Rcc_Set_PeriphActive_ExpectAndReturn( periphId, RCC_REQUEST_OK );
}


/**
 * \brief Activates interrupt of the channel and returns handler registered to NVIC.
 *
 * \param dmaBus     [in]: DMA peripheral ID
 * \param dmaChannel [in]: DMA channel ID
 * \param irqId      [in]: Expected NVIC interrupt of the channel
 * \return Interrupt handler of the channel
 */
static nvic_IsrCallback_t Ut_Dma_Capture_IsrHandler( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, nvic_PeriphIrqList_t irqId )
{
    Nvic_Set_PeriphIrq_Handler_ExpectAndReturn( irqId, NULL, NVIC_REQUEST_OK );
    Nvic_Set_PeriphIrq_Handler_IgnoreArg_irqHandler();
    Nvic_Set_PeriphIrq_Handler_AddCallback( Ut_Dma_Nvic_HandlerCallback );
    Nvic_Set_PeriphIrq_Active_ExpectAndReturn( irqId, NVIC_REQUEST_OK );

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_InterruptActive( dmaBus, dmaChannel ) );
    TEST_ASSERT_NOT_NULL( utDma_CapturedIsr );

    return ( utDma_CapturedIsr );
}


/**
 * \brief NVIC mock callback storing registered interrupt handler.
 */
static nvic_RequestState_t Ut_Dma_Nvic_HandlerCallback( nvic_PeriphIrqList_t irqId, const nvic_IsrCallback_t irqHandler, int cmockNumCalls )
{
    (void)irqId;
    (void)cmockNumCalls;

    utDma_CapturedIsr = irqHandler;

    return ( NVIC_REQUEST_OK );
}


/** \brief Transfer complete callback recording execution sequence */
static void Ut_Dma_TransferCompleteCallback( void )
{
    utDma_TransferCompleteSeq = ++utDma_CallbackCnt;
}


/** \brief Half transfer callback recording execution sequence */
static void Ut_Dma_HalfTransferCallback( void )
{
    utDma_HalfTransferSeq = ++utDma_CallbackCnt;
}


/** \brief Transfer error callback recording execution sequence */
static void Ut_Dma_TransferErrorCallback( void )
{
    utDma_TransferErrorSeq = ++utDma_CallbackCnt;
}
