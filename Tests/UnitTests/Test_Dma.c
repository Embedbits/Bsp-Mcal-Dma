/**
 * \author Mr.Nobody
 * \file Test_Dma.c
 * \ingroup Dma
 * \brief Unit tests of Direct Memory Access (DMA) module.
 *
 * Dma.c is compiled unchanged with real LL drivers. DMA registers are emulated
 * by RegMem (host memory at MCU addresses), RCC and NVIC modules are mocked by
 * CMock.
 *
 * \note Emulated registers are plain memory. Writes to flag clear registers
 *       (LIFCR / HIFCR) do not clear flags in LISR / HISR, tests check the
 *       value written to the clear register. Stream enable bit (EN) is not
 *       cleared by hardware at the end of transfer.
 * \note STM32F4 DMA has 8 streams per DMA peripheral, module "channel" is DMA
 *       stream, peripheral request is channel selection (CHSEL) of the stream.
 */

/* ============================= INCLUDES =================================== */
#include <string.h>                         /* memset                         */
#include "unity.h"                          /* Unity testing framework        */
#include "RegMem.h"                         /* Register memory emulation      */
#include "Dma_Port.h"                       /* Module under test              */
#include "MockRcc_Port.h"                   /* RCC module mock                */
#include "MockNvic_Port.h"                  /* NVIC module mock               */
#include "Stm32_dma.h"                      /* DMA registers definition       */
/* ============================= TYPEDEFS =================================== */

/* ======================= FORWARD DECLARATIONS ============================= */

static void                 Ut_Dma_Expect_ClockState        ( rcc_PeriphId_t periphId, rcc_FunctionState_t clockState );
static void                 Ut_Dma_Expect_ClockActivation   ( rcc_PeriphId_t periphId );
static nvic_IsrCallback_t   Ut_Dma_Capture_IsrHandler       ( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, nvic_PeriphIrqList_t irqId );
static nvic_RequestState_t  Ut_Dma_Nvic_HandlerCallback     ( nvic_PeriphIrqList_t irqId, const nvic_IsrCallback_t irqHandler, int cmockNumCalls );
static void                 Ut_Dma_TransferCompleteCallback ( void );
static void                 Ut_Dma_HalfTransferCallback     ( void );
static void                 Ut_Dma_TransferErrorCallback    ( void );

/* ========================= SYMBOLIC CONSTANTS ============================= */

/** All flags of stream 0 (FEIF, DMEIF, TEIF, HTIF, TCIF), other streams are shifted */
#define TEST_DMA_STREAM0_FLAGS              ( DMA_LIFCR_CFEIF0 | DMA_LIFCR_CDMEIF0 | DMA_LIFCR_CTEIF0 | \
                                              DMA_LIFCR_CHTIF0 | DMA_LIFCR_CTCIF0 )

/** Peripheral (source) address used by tests */
#define TEST_DMA_PERIPH_ADDR                ( 0x20000100u )

/** Memory (destination) address used by tests */
#define TEST_DMA_MEMORY_ADDR                ( 0x20000200u )

/* ============================== MACROS ==================================== */

/* ========================== LOCAL VARIABLES =============================== */

/** Stream registers of every DMA peripheral and stream */
static DMA_Stream_TypeDef * const utDma_StreamRegLut[ DMA_PERIPH_CNT ][ DMA_STREAM_CNT ] =
{
    { DMA1_Stream0, DMA1_Stream1, DMA1_Stream2, DMA1_Stream3, DMA1_Stream4, DMA1_Stream5, DMA1_Stream6, DMA1_Stream7 },
    { DMA2_Stream0, DMA2_Stream1, DMA2_Stream2, DMA2_Stream3, DMA2_Stream4, DMA2_Stream5, DMA2_Stream6, DMA2_Stream7 },
};

/** Interrupt handler of the stream registered to NVIC mock */
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
 * - DMA1 stream 0, peripheral to memory, normal mode, static addresses 0,
 *   8 bit transfers, 0 data, channel 0, low priority, no callbacks.
 */
void Ut_Dma_Get_DefaultConfig_FillsDefaults( void )
{
    dma_ConfigStruct_t dmaConfig;

    (void)memset( &dmaConfig, 0xA5, sizeof( dmaConfig ) );

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Get_DefaultConfig( &dmaConfig ) );

    TEST_ASSERT_EQUAL( DMA_PERIPH_1,               dmaConfig.DmaPeriphId );
    TEST_ASSERT_EQUAL( DMA_STREAM_0,               dmaConfig.DmaChannel );
    TEST_ASSERT_EQUAL( DMA_DIR_PERIPH_TO_MEMORY,   dmaConfig.Direction );
    TEST_ASSERT_EQUAL( DMA_TRANSFER_MODE_NORMAL,   dmaConfig.TransferMode );
    TEST_ASSERT_EQUAL_HEX32( 0u,                   dmaConfig.PeriphAddress );
    TEST_ASSERT_EQUAL_HEX32( 0u,                   dmaConfig.MemoryAddress );
    TEST_ASSERT_EQUAL( DMA_PERIPH_ADDR_STATIC,     dmaConfig.PeriphAddrIncrement );
    TEST_ASSERT_EQUAL( DMA_MEMORY_ADDR_STATIC,     dmaConfig.MemoryAddrIncrement );
    TEST_ASSERT_EQUAL( DMA_TRANSFER_SIZE_8BIT,     dmaConfig.PeriphTransferSize );
    TEST_ASSERT_EQUAL( DMA_TRANSFER_SIZE_8BIT,     dmaConfig.MemoryTransferSize );
    TEST_ASSERT_EQUAL_UINT32( 0u,                  dmaConfig.DataCount );
    TEST_ASSERT_EQUAL( DMA_REQ_CHANNEL_0,          dmaConfig.PeripheralReqId );
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
 * \brief   Dma_Init() activates inactive clock and configures the stream.
 *
 * \details DMA2 stream 0, memory to memory, both addresses incremented, 32 bit
 *          transfers, 16 data, very high priority, all callbacks configured.
 *
 * \par Expected results
 * - RCC clock of DMA2 is read and activated.
 * - CR contains direction, increments, sizes and priority, stream disabled.
 * - PAR, M0AR, NDTR hold source, destination and data count.
 * - Direct mode (equal sizes) - FCR DMDIS cleared.
 * - Callbacks are stored.
 */
void Ut_Dma_Init_ClockInactive_ActivatesClockAndConfiguresStream( void )
{
    dma_ConfigStruct_t dmaConfig;
    dma_IsrCallback    callback = NULL;

    (void)Dma_Get_DefaultConfig( &dmaConfig );
    dmaConfig.DmaPeriphId              = DMA_PERIPH_2;
    dmaConfig.DmaChannel               = DMA_STREAM_0;
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

    Ut_Dma_Expect_ClockActivation( RCC_PERIPH_DMA2 );

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Init( &dmaConfig ) );

    TEST_ASSERT_EQUAL_HEX32( DMA_SxCR_DIR_1 | DMA_SxCR_PINC | DMA_SxCR_MINC | DMA_SxCR_PSIZE_1 |
                             DMA_SxCR_MSIZE_1 | DMA_SxCR_PL, DMA2_Stream0->CR );
    TEST_ASSERT_EQUAL_HEX32( TEST_DMA_PERIPH_ADDR, DMA2_Stream0->PAR );
    TEST_ASSERT_EQUAL_HEX32( TEST_DMA_MEMORY_ADDR, DMA2_Stream0->M0AR );
    TEST_ASSERT_EQUAL_UINT32( 16u, DMA2_Stream0->NDTR );
    TEST_ASSERT_EQUAL_HEX32( 0u, DMA2_Stream0->FCR & DMA_SxFCR_DMDIS );

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Get_TransferCompleteIsrHandler( DMA_PERIPH_2, DMA_STREAM_0, &callback ) );
    TEST_ASSERT_EQUAL_PTR( Ut_Dma_TransferCompleteCallback, callback );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Get_HalfTransferIsrHandler( DMA_PERIPH_2, DMA_STREAM_0, &callback ) );
    TEST_ASSERT_EQUAL_PTR( Ut_Dma_HalfTransferCallback, callback );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Get_TransferErrorIsrHandler( DMA_PERIPH_2, DMA_STREAM_0, &callback ) );
    TEST_ASSERT_EQUAL_PTR( Ut_Dma_TransferErrorCallback, callback );
}


/**
 * \brief   Dma_Init() does not activate already active clock.
 *
 * \details RCC mock reports active DMA1 clock, default configuration of DMA1
 *          stream 0 is initialized.
 *
 * \par Expected results
 * - Only clock state is read, Rcc_Set_PeriphActive() is not called.
 * - DMA_REQUEST_OK is returned.
 */
void Ut_Dma_Init_ClockActive_DoesNotReactivate( void )
{
    dma_ConfigStruct_t dmaConfig;

    (void)Dma_Get_DefaultConfig( &dmaConfig );

    Ut_Dma_Expect_ClockState( RCC_PERIPH_DMA1, RCC_FUNCTION_ACTIVE );

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Init( &dmaConfig ) );
}


/**
 * \brief   Dma_Init() enables FIFO when peripheral and memory sizes differ.
 *
 * \details DMA1 stream 5 channel 4 (USART2_RX on STM32F407), 8 bit peripheral,
 *          32 bit memory.
 *
 * \par Expected results
 * - FCR DMDIS set (FIFO mode), FIFO threshold full.
 * - CR contains channel selection 4, PSIZE byte and MSIZE word.
 */
void Ut_Dma_Init_DifferentSizes_EnablesFifo( void )
{
    dma_ConfigStruct_t dmaConfig;

    (void)Dma_Get_DefaultConfig( &dmaConfig );
    dmaConfig.DmaChannel         = DMA_STREAM_5;
    dmaConfig.PeripheralReqId    = DMA_REQ_CHANNEL_4;
    dmaConfig.PeriphTransferSize = DMA_TRANSFER_SIZE_8BIT;
    dmaConfig.MemoryTransferSize = DMA_TRANSFER_SIZE_32BIT;

    Ut_Dma_Expect_ClockState( RCC_PERIPH_DMA1, RCC_FUNCTION_ACTIVE );

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Init( &dmaConfig ) );

    TEST_ASSERT_EQUAL_HEX32( DMA_SxFCR_DMDIS | DMA_SxFCR_FTH, DMA1_Stream5->FCR & ( DMA_SxFCR_DMDIS | DMA_SxFCR_FTH ) );
    TEST_ASSERT_EQUAL_HEX32( LL_DMA_CHANNEL_4, DMA1_Stream5->CR & DMA_SxCR_CHSEL );
    TEST_ASSERT_EQUAL_HEX32( DMA_SxCR_MSIZE_1, DMA1_Stream5->CR & ( DMA_SxCR_PSIZE | DMA_SxCR_MSIZE ) );
}


/**
 * \brief   Dma_Init() disables enabled stream before configuration.
 *
 * \details Stream is enabled with transfer complete interrupt.
 *
 * \par Expected results
 * - DMA_REQUEST_OK, stream is disabled (EN cleared), interrupt enable kept.
 * - Stream configuration is written (PAR).
 */
void Ut_Dma_Init_StreamEnabled_DisablesStreamFirst( void )
{
    dma_ConfigStruct_t dmaConfig;

    (void)Dma_Get_DefaultConfig( &dmaConfig );
    dmaConfig.PeriphAddress = TEST_DMA_PERIPH_ADDR;
    DMA1_Stream0->CR        = DMA_SxCR_EN | DMA_SxCR_TCIE;

    Ut_Dma_Expect_ClockState( RCC_PERIPH_DMA1, RCC_FUNCTION_ACTIVE );

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Init( &dmaConfig ) );

    TEST_ASSERT_EQUAL_HEX32( 0u, DMA1_Stream0->CR & DMA_SxCR_EN );
    TEST_ASSERT_EQUAL_HEX32( DMA_SxCR_TCIE, DMA1_Stream0->CR & DMA_SxCR_TCIE );
    TEST_ASSERT_EQUAL_HEX32( TEST_DMA_PERIPH_ADDR, DMA1_Stream0->PAR );
}


/**
 * \brief   Dma_Init() rejects memory to memory direction on DMA1.
 *
 * \details Memory to memory transfer is supported by DMA2 only.
 *
 * \par Expected results
 * - DMA_REQUEST_ERROR is returned.
 */
void Ut_Dma_Init_MemToMemOnDma1_ReturnsError( void )
{
    dma_ConfigStruct_t dmaConfig;

    (void)Dma_Get_DefaultConfig( &dmaConfig );
    dmaConfig.Direction = DMA_DIR_MEMORY_TO_MEMORY;

    Ut_Dma_Expect_ClockState( RCC_PERIPH_DMA1, RCC_FUNCTION_ACTIVE );

    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Init( &dmaConfig ) );
}


/**
 * \brief   Dma_Init() rejects data count above 16 bit NDTR range.
 *
 * \par Expected results
 * - DMA_REQUEST_ERROR is returned.
 */
void Ut_Dma_Init_DataCountOutOfRange_ReturnsError( void )
{
    dma_ConfigStruct_t dmaConfig;

    (void)Dma_Get_DefaultConfig( &dmaConfig );
    dmaConfig.DataCount = DMA_DATA_COUNT_MAX + 1u;

    Ut_Dma_Expect_ClockState( RCC_PERIPH_DMA1, RCC_FUNCTION_ACTIVE );

    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Init( &dmaConfig ) );
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
 * \brief   Dma_Init() reports error of clock activation.
 *
 * \par Expected results
 * - DMA_REQUEST_ERROR is returned, stream is not configured.
 */
void Ut_Dma_Init_RccActivationError_ReturnsError( void )
{
    dma_ConfigStruct_t  dmaConfig;
    rcc_FunctionState_t clockState = RCC_FUNCTION_INACTIVE;

    (void)Dma_Get_DefaultConfig( &dmaConfig );
    dmaConfig.PeriphAddress = TEST_DMA_PERIPH_ADDR;

    Rcc_Get_PeriphState_ExpectAndReturn( RCC_PERIPH_DMA1, NULL, RCC_REQUEST_OK );
    Rcc_Get_PeriphState_IgnoreArg_funcState();
    Rcc_Get_PeriphState_ReturnThruPtr_funcState( &clockState );
    Rcc_Set_PeriphActive_ExpectAndReturn( RCC_PERIPH_DMA1, RCC_REQUEST_ERROR );

    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Init( &dmaConfig ) );
    TEST_ASSERT_EQUAL_HEX32( 0u, DMA1_Stream0->PAR );
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
    dmaConfig.DmaChannel = DMA_STREAM_CNT;
    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Init( &dmaConfig ) );
}

/* ========================= TRANSFER ACTIVATION ============================ */

/**
 * \brief   Dma_Set_TransferActive() clears stream flags and enables stream.
 *
 * \details Streams with all flag offsets: 0, 1 (offset 6), 2 (offset 16),
 *          3 (offset 22) in LIFCR and 4 - 7 in HIFCR, on both DMA peripherals.
 *
 * \par Expected results
 * - DMA_REQUEST_OK, EN set.
 * - All five flags of the stream written to LIFCR (streams 0-3) or HIFCR (4-7).
 */
void Ut_Dma_Set_TransferActive_AllStreams_ClearsFlagsAndEnables( void )
{
    static const uint32_t flagOffsetLut[ DMA_STREAM_CNT ] = { 0u, 6u, 16u, 22u, 0u, 6u, 16u, 22u };
    DMA_TypeDef * const   dmaRegLut[ DMA_PERIPH_CNT ]     = { DMA1, DMA2 };

    for( uint32_t dmaBus = 0u; DMA_PERIPH_CNT > dmaBus; dmaBus++ )
    {
        for( uint32_t stream = 0u; DMA_STREAM_CNT > stream; stream++ )
        {
            TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Reset() );

            TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_TransferActive( (dma_PeriphId_t)dmaBus, (dma_ChannelId_t)stream ) );

            TEST_ASSERT_EQUAL_HEX32( DMA_SxCR_EN, utDma_StreamRegLut[ dmaBus ][ stream ]->CR & DMA_SxCR_EN );

            if( 4u > stream )
            {
                TEST_ASSERT_EQUAL_HEX32( TEST_DMA_STREAM0_FLAGS << flagOffsetLut[ stream ], dmaRegLut[ dmaBus ]->LIFCR );
                TEST_ASSERT_EQUAL_HEX32( 0u, dmaRegLut[ dmaBus ]->HIFCR );
            }
            else
            {
                TEST_ASSERT_EQUAL_HEX32( TEST_DMA_STREAM0_FLAGS << flagOffsetLut[ stream ], dmaRegLut[ dmaBus ]->HIFCR );
                TEST_ASSERT_EQUAL_HEX32( 0u, dmaRegLut[ dmaBus ]->LIFCR );
            }
        }
    }
}


/**
 * \brief   Dma_Set_TransferActive() reports transfer error.
 *
 * \details Transfer error flag of DMA1 stream 2 stays active after enable
 *          (emulated register is not cleared by LIFCR write).
 *
 * \par Expected results
 * - DMA_REQUEST_ERROR is returned.
 */
void Ut_Dma_Set_TransferActive_TransferError_ReturnsError( void )
{
    DMA1->LISR = DMA_LISR_TEIF2;

    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_TransferActive( DMA_PERIPH_1, DMA_STREAM_2 ) );
}


/**
 * \brief   Dma_Set_TransferActive() refuses DMA2 peripheral port accesses affected by the errata.
 *
 * \details DMA2 stream 5: memory to peripheral to GPIOB BSRR, peripheral to memory from GPIOB
 *          IDR, peripheral to memory from FSMC bank 1, workaround configuration (peripheral to
 *          memory from SRAM, memory address GPIOB BSRR), DMA1 stream 5 memory to peripheral to
 *          GPIOB BSRR.
 *
 * \note    Device errata bug AB#657 (ES0182 2.2.19 "In some specific cases, DMA2 data corruption
 *          occurs when managing AHB and APB2 peripherals in a concurrent way"): accesses of the
 *          DMA2 AHB peripheral port may be performed several times - GPIO writes and FSMC /
 *          QUADSPI accesses through it are corrupted. Workaround - access through the memory
 *          port. Devices without the errata (STM32F42x / F446 / F469): test ignored.
 *
 * \par Expected results
 * - DMA2 GPIO write and FSMC access through the peripheral port: DMA_REQUEST_ERROR, stream not
 *   enabled.
 * - GPIO read through the peripheral port, workaround configuration and DMA1: stream enabled.
 */
void Ut_Dma_Set_TransferActive_Dma2AhbPeriphPort_Refused( void )
{
#if defined(STM32F401xC) || defined(STM32F401xE) || defined(STM32F405xx) || defined(STM32F407xx) || \
    defined(STM32F410Cx) || defined(STM32F410Rx) || defined(STM32F410Tx) || defined(STM32F411xE) || \
    defined(STM32F412Cx) || defined(STM32F412Rx) || defined(STM32F412Vx) || defined(STM32F412Zx) || \
    defined(STM32F413xx) || defined(STM32F415xx) || defined(STM32F417xx) || defined(STM32F423xx)
    DMA_Stream_TypeDef * const stream = DMA2_Stream5;

    /* GPIO write through the peripheral port */
    stream->CR  = LL_DMA_DIRECTION_MEMORY_TO_PERIPH;
    stream->PAR = (uint32_t)(uintptr_t)&GPIOB->BSRR;
    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_TransferActive( DMA_PERIPH_2, DMA_STREAM_5 ) );
    TEST_ASSERT_EQUAL_HEX32( 0u, stream->CR & DMA_SxCR_EN );

    /* FSMC bank 1 through the peripheral port */
    stream->CR  = LL_DMA_DIRECTION_PERIPH_TO_MEMORY;
    stream->PAR = 0x60000000u;
    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_TransferActive( DMA_PERIPH_2, DMA_STREAM_5 ) );
    TEST_ASSERT_EQUAL_HEX32( 0u, stream->CR & DMA_SxCR_EN );

    /* GPIO read through the peripheral port is not affected */
    stream->PAR = (uint32_t)(uintptr_t)&GPIOB->IDR;
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_TransferActive( DMA_PERIPH_2, DMA_STREAM_5 ) );

    /* Workaround: GPIO written through the memory port */
    stream->CR   = LL_DMA_DIRECTION_PERIPH_TO_MEMORY;
    stream->PAR  = REGMEM_SRAM_BASE;
    stream->M0AR = (uint32_t)(uintptr_t)&GPIOB->BSRR;
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_TransferActive( DMA_PERIPH_2, DMA_STREAM_5 ) );

    /* DMA1 is not affected */
    DMA1_Stream5->CR  = LL_DMA_DIRECTION_MEMORY_TO_PERIPH;
    DMA1_Stream5->PAR = (uint32_t)(uintptr_t)&GPIOB->BSRR;
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_TransferActive( DMA_PERIPH_1, DMA_STREAM_5 ) );
#else
    TEST_IGNORE_MESSAGE( "Device errata not listed for this device" );
#endif /* Devices listing the errata */
}


/**
 * \brief   Dma_Set_TransferInactive() disables stream and clears its flags.
 *
 * \details DMA1 stream 0 is enabled with transfer complete and half transfer
 *          interrupts.
 *
 * \par Expected results
 * - DMA_REQUEST_OK, EN cleared, interrupt enables restored.
 * - All flags of stream 0 written to LIFCR.
 */
void Ut_Dma_Set_TransferInactive_EnabledStream_DisablesAndClearsFlags( void )
{
    DMA1_Stream0->CR = DMA_SxCR_EN | DMA_SxCR_TCIE | DMA_SxCR_HTIE;

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_TransferInactive( DMA_PERIPH_1, DMA_STREAM_0 ) );

    TEST_ASSERT_EQUAL_HEX32( DMA_SxCR_TCIE | DMA_SxCR_HTIE, DMA1_Stream0->CR );
    TEST_ASSERT_EQUAL_HEX32( TEST_DMA_STREAM0_FLAGS, DMA1->LIFCR );
}


/**
 * \brief   Dma_Get_TransferState() reports stream enable state.
 *
 * \par Expected results
 * - EN set: DMA_FUNCTION_ACTIVE, EN cleared: DMA_FUNCTION_INACTIVE.
 */
void Ut_Dma_Get_TransferState_ReportsEnableBit( void )
{
    dma_FunctionState_t state = DMA_FUNCTION_INACTIVE;

    DMA2_Stream7->CR = DMA_SxCR_EN;
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Get_TransferState( DMA_PERIPH_2, DMA_STREAM_7, &state ) );
    TEST_ASSERT_EQUAL( DMA_FUNCTION_ACTIVE, state );

    DMA2_Stream7->CR = 0u;
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Get_TransferState( DMA_PERIPH_2, DMA_STREAM_7, &state ) );
    TEST_ASSERT_EQUAL( DMA_FUNCTION_INACTIVE, state );
}


/**
 * \brief   Channel activation functions control the stream like transfer
 *          activation functions.
 *
 * \par Expected results
 * - Dma_Set_ChannelActive() sets EN, Dma_Get_ChannelState() reports active.
 * - Dma_Set_ChannelInactive() clears EN, Dma_Get_ChannelState() reports inactive.
 */
void Ut_Dma_Set_ChannelActive_ControlsStreamEnable( void )
{
    dma_FunctionState_t state = DMA_FUNCTION_INACTIVE;

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_ChannelActive( DMA_PERIPH_1, DMA_STREAM_3 ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Get_ChannelState( DMA_PERIPH_1, DMA_STREAM_3, &state ) );
    TEST_ASSERT_EQUAL( DMA_FUNCTION_ACTIVE, state );

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_ChannelInactive( DMA_PERIPH_1, DMA_STREAM_3 ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Get_ChannelState( DMA_PERIPH_1, DMA_STREAM_3, &state ) );
    TEST_ASSERT_EQUAL( DMA_FUNCTION_INACTIVE, state );
}

/* ======================= STREAM CONFIGURATION ============================= */

/**
 * \brief   Transfer direction is written to CR and read back.
 *
 * \details All directions on DMA2 stream 1.
 *
 * \par Expected results
 * - DIR field holds requested direction, getter returns it.
 */
void Ut_Dma_Set_Direction_AllDirections_RoundTrip( void )
{
    static const dma_Direction_t directionLut[] = { DMA_DIR_MEMORY_TO_PERIPH, DMA_DIR_MEMORY_TO_MEMORY, DMA_DIR_PERIPH_TO_MEMORY };
    dma_Direction_t direction = DMA_DIR_PERIPH_TO_MEMORY;

    for( uint32_t idx = 0u; ( sizeof( directionLut ) / sizeof( directionLut[ 0u ] ) ) > idx; idx++ )
    {
        TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_Direction( DMA_PERIPH_2, DMA_STREAM_1, directionLut[ idx ] ) );
        TEST_ASSERT_EQUAL_HEX32( directionLut[ idx ], DMA2_Stream1->CR & DMA_SxCR_DIR );

        TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Get_Direction( DMA_PERIPH_2, DMA_STREAM_1, &direction ) );
        TEST_ASSERT_EQUAL( directionLut[ idx ], direction );
    }
}


/**
 * \brief   Memory to memory direction is rejected on DMA1.
 *
 * \par Expected results
 * - DMA_REQUEST_ERROR, CR not changed.
 */
void Ut_Dma_Set_Direction_MemToMemOnDma1_ReturnsError( void )
{
    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_Direction( DMA_PERIPH_1, DMA_STREAM_1, DMA_DIR_MEMORY_TO_MEMORY ) );
    TEST_ASSERT_EQUAL_HEX32( 0u, DMA1_Stream1->CR );
}


/**
 * \brief   Transfer mode is written to CR and read back.
 *
 * \par Expected results
 * - CIRC set for circular mode and cleared for normal mode.
 */
void Ut_Dma_Set_TransferMode_RoundTrip( void )
{
    dma_TransferMode_t mode = DMA_TRANSFER_MODE_NORMAL;

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_TransferMode( DMA_PERIPH_1, DMA_STREAM_6, DMA_TRANSFER_MODE_CIRCULAR ) );
    TEST_ASSERT_EQUAL_HEX32( DMA_SxCR_CIRC, DMA1_Stream6->CR );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Get_TransferMode( DMA_PERIPH_1, DMA_STREAM_6, &mode ) );
    TEST_ASSERT_EQUAL( DMA_TRANSFER_MODE_CIRCULAR, mode );

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_TransferMode( DMA_PERIPH_1, DMA_STREAM_6, DMA_TRANSFER_MODE_NORMAL ) );
    TEST_ASSERT_EQUAL_HEX32( 0u, DMA1_Stream6->CR );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Get_TransferMode( DMA_PERIPH_1, DMA_STREAM_6, &mode ) );
    TEST_ASSERT_EQUAL( DMA_TRANSFER_MODE_NORMAL, mode );
}


/**
 * \brief   Peripheral and memory addresses are written to PAR / M0AR.
 *
 * \par Expected results
 * - PAR holds peripheral address, M0AR memory address, getters return them
 *   (memory address getter reads M0AR, not PAR).
 */
void Ut_Dma_Set_Addresses_RoundTrip( void )
{
    dma_PeriphAddr_t periphAddr = 0u;
    dma_MemoryAddr_t memoryAddr = 0u;

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_PeriphAddr( DMA_PERIPH_2, DMA_STREAM_4, TEST_DMA_PERIPH_ADDR ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_MemoryAddr( DMA_PERIPH_2, DMA_STREAM_4, TEST_DMA_MEMORY_ADDR ) );

    TEST_ASSERT_EQUAL_HEX32( TEST_DMA_PERIPH_ADDR, DMA2_Stream4->PAR );
    TEST_ASSERT_EQUAL_HEX32( TEST_DMA_MEMORY_ADDR, DMA2_Stream4->M0AR );

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Get_PeriphAddr( DMA_PERIPH_2, DMA_STREAM_4, &periphAddr ) );
    TEST_ASSERT_EQUAL_HEX32( TEST_DMA_PERIPH_ADDR, periphAddr );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Get_MemoryAddr( DMA_PERIPH_2, DMA_STREAM_4, &memoryAddr ) );
    TEST_ASSERT_EQUAL_HEX32( TEST_DMA_MEMORY_ADDR, memoryAddr );
}


/**
 * \brief   Address increments are written to CR and read back.
 *
 * \par Expected results
 * - PINC / MINC follow requested increment, getters return it.
 */
void Ut_Dma_Set_AddrIncrement_RoundTrip( void )
{
    dma_PeriphAddrInc_t periphInc = DMA_PERIPH_ADDR_STATIC;
    dma_MemoryAddrInc_t memoryInc = DMA_MEMORY_ADDR_STATIC;

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_PeriphAddrIncrement( DMA_PERIPH_1, DMA_STREAM_7, DMA_PERIPH_ADDR_INCREMENT ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_MemoryAddrIncrement( DMA_PERIPH_1, DMA_STREAM_7, DMA_MEMORY_ADDR_INCREMENT ) );
    TEST_ASSERT_EQUAL_HEX32( DMA_SxCR_PINC | DMA_SxCR_MINC, DMA1_Stream7->CR );

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Get_PeriphAddrIncrement( DMA_PERIPH_1, DMA_STREAM_7, &periphInc ) );
    TEST_ASSERT_EQUAL( DMA_PERIPH_ADDR_INCREMENT, periphInc );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Get_MemoryAddrIncrement( DMA_PERIPH_1, DMA_STREAM_7, &memoryInc ) );
    TEST_ASSERT_EQUAL( DMA_MEMORY_ADDR_INCREMENT, memoryInc );

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_PeriphAddrIncrement( DMA_PERIPH_1, DMA_STREAM_7, DMA_PERIPH_ADDR_STATIC ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_MemoryAddrIncrement( DMA_PERIPH_1, DMA_STREAM_7, DMA_MEMORY_ADDR_STATIC ) );
    TEST_ASSERT_EQUAL_HEX32( 0u, DMA1_Stream7->CR );
}


/**
 * \brief   Transfer sizes are written to CR and read back.
 *
 * \details All sizes on peripheral and memory side.
 *
 * \par Expected results
 * - PSIZE / MSIZE hold byte (0), half word (1), word (2), getters return sizes.
 */
void Ut_Dma_Set_TransferSize_AllSizes_RoundTrip( void )
{
    static const uint32_t psizeLut[ DMA_TRANSFER_SIZE_CNT ] = { 0u, DMA_SxCR_PSIZE_0, DMA_SxCR_PSIZE_1 };
    static const uint32_t msizeLut[ DMA_TRANSFER_SIZE_CNT ] = { 0u, DMA_SxCR_MSIZE_0, DMA_SxCR_MSIZE_1 };
    dma_TransferSize_t    readSize = DMA_TRANSFER_SIZE_8BIT;

    for( uint32_t size = 0u; DMA_TRANSFER_SIZE_CNT > size; size++ )
    {
        TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_PeriphTransferSize( DMA_PERIPH_2, DMA_STREAM_2, (dma_TransferSize_t)size ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_MemoryTransferSize( DMA_PERIPH_2, DMA_STREAM_2, (dma_TransferSize_t)size ) );
        TEST_ASSERT_EQUAL_HEX32( psizeLut[ size ] | msizeLut[ size ], DMA2_Stream2->CR );

        TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Get_PeriphTransferSize( DMA_PERIPH_2, DMA_STREAM_2, &readSize ) );
        TEST_ASSERT_EQUAL( size, readSize );
        TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Get_MemoryTransferSize( DMA_PERIPH_2, DMA_STREAM_2, &readSize ) );
        TEST_ASSERT_EQUAL( size, readSize );
    }

    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_PeriphTransferSize( DMA_PERIPH_2, DMA_STREAM_2, DMA_TRANSFER_SIZE_CNT ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_MemoryTransferSize( DMA_PERIPH_2, DMA_STREAM_2, DMA_TRANSFER_SIZE_CNT ) );
}


/**
 * \brief   Data count is written to NDTR and read back, range is checked.
 *
 * \par Expected results
 * - 0xFFFF accepted, 0x10000 rejected (NDTR unchanged).
 */
void Ut_Dma_Set_DataCount_RangeChecked( void )
{
    dma_DataCount_t dataCount = 0u;

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_DataCount( DMA_PERIPH_1, DMA_STREAM_1, DMA_DATA_COUNT_MAX ) );
    TEST_ASSERT_EQUAL_HEX32( DMA_DATA_COUNT_MAX, DMA1_Stream1->NDTR );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Get_DataCount( DMA_PERIPH_1, DMA_STREAM_1, &dataCount ) );
    TEST_ASSERT_EQUAL_UINT32( DMA_DATA_COUNT_MAX, dataCount );

    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_DataCount( DMA_PERIPH_1, DMA_STREAM_1, DMA_DATA_COUNT_MAX + 1u ) );
    TEST_ASSERT_EQUAL_HEX32( DMA_DATA_COUNT_MAX, DMA1_Stream1->NDTR );
}


/**
 * \brief   Peripheral request is written to CHSEL and read back.
 *
 * \par Expected results
 * - CHSEL holds channel 7, getter returns DMA_REQ_CHANNEL_7.
 * - Value outside of CHSEL field is rejected.
 */
void Ut_Dma_Set_PeripheralRequest_RoundTrip( void )
{
    dma_PeriphReqId_t requestId = DMA_REQ_CHANNEL_0;

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_PeripheralRequest( DMA_PERIPH_1, DMA_STREAM_4, DMA_REQ_CHANNEL_7 ) );
    TEST_ASSERT_EQUAL_HEX32( DMA_SxCR_CHSEL_0 | DMA_SxCR_CHSEL_1 | DMA_SxCR_CHSEL_2, DMA1_Stream4->CR );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Get_PeripheralRequest( DMA_PERIPH_1, DMA_STREAM_4, &requestId ) );
    TEST_ASSERT_EQUAL( DMA_REQ_CHANNEL_7, requestId );

    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_PeripheralRequest( DMA_PERIPH_1, DMA_STREAM_4, (dma_PeriphReqId_t)DMA_SxCR_EN ) );
}


/**
 * \brief   Stream priority is written to PL and read back.
 *
 * \par Expected results
 * - PL holds 0 - 3 for low - very high priority, getter returns priority.
 * - DMA_PRIORITY_CNT is rejected.
 */
void Ut_Dma_Set_Priority_AllLevels_RoundTrip( void )
{
    dma_Priority_t priority = DMA_PRIORITY_LOW;

    for( uint32_t level = 0u; DMA_PRIORITY_CNT > level; level++ )
    {
        TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_Priority( DMA_PERIPH_2, DMA_STREAM_3, (dma_Priority_t)level ) );
        TEST_ASSERT_EQUAL_HEX32( level << DMA_SxCR_PL_Pos, DMA2_Stream3->CR );
        TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Get_Priority( DMA_PERIPH_2, DMA_STREAM_3, &priority ) );
        TEST_ASSERT_EQUAL( level, priority );
    }

    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_Priority( DMA_PERIPH_2, DMA_STREAM_3, DMA_PRIORITY_CNT ) );
}


/**
 * \brief   Configuration of enabled stream is rejected.
 *
 * \details Stream configuration registers are write protected by hardware
 *          while EN is set.
 *
 * \par Expected results
 * - All configuration setters return DMA_REQUEST_ERROR.
 * - CR, PAR, M0AR, NDTR are not changed.
 */
void Ut_Dma_Set_Configuration_StreamEnabled_ReturnsError( void )
{
    DMA2_Stream0->CR = DMA_SxCR_EN;

    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_Direction( DMA_PERIPH_2, DMA_STREAM_0, DMA_DIR_MEMORY_TO_PERIPH ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_TransferMode( DMA_PERIPH_2, DMA_STREAM_0, DMA_TRANSFER_MODE_CIRCULAR ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_PeriphAddr( DMA_PERIPH_2, DMA_STREAM_0, TEST_DMA_PERIPH_ADDR ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_MemoryAddr( DMA_PERIPH_2, DMA_STREAM_0, TEST_DMA_MEMORY_ADDR ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_PeriphAddrIncrement( DMA_PERIPH_2, DMA_STREAM_0, DMA_PERIPH_ADDR_INCREMENT ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_MemoryAddrIncrement( DMA_PERIPH_2, DMA_STREAM_0, DMA_MEMORY_ADDR_INCREMENT ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_PeriphTransferSize( DMA_PERIPH_2, DMA_STREAM_0, DMA_TRANSFER_SIZE_32BIT ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_MemoryTransferSize( DMA_PERIPH_2, DMA_STREAM_0, DMA_TRANSFER_SIZE_32BIT ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_DataCount( DMA_PERIPH_2, DMA_STREAM_0, 10u ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_PeripheralRequest( DMA_PERIPH_2, DMA_STREAM_0, DMA_REQ_CHANNEL_3 ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_Priority( DMA_PERIPH_2, DMA_STREAM_0, DMA_PRIORITY_HIGH ) );

    TEST_ASSERT_EQUAL_HEX32( DMA_SxCR_EN, DMA2_Stream0->CR );
    TEST_ASSERT_EQUAL_HEX32( 0u, DMA2_Stream0->PAR );
    TEST_ASSERT_EQUAL_HEX32( 0u, DMA2_Stream0->M0AR );
    TEST_ASSERT_EQUAL_HEX32( 0u, DMA2_Stream0->NDTR );
}


/**
 * \brief   All functions reject invalid DMA peripheral and stream.
 *
 * \par Expected results
 * - DMA_REQUEST_ERROR for DMA_PERIPH_CNT and DMA_STREAM_CNT, registers of all
 *   streams are not changed.
 */
void Ut_Dma_InvalidIdentifiers_ReturnError( void )
{
    static const dma_PeriphId_t  busLut[]    = { DMA_PERIPH_CNT, DMA_PERIPH_1 };
    static const dma_ChannelId_t streamLut[] = { DMA_STREAM_0, DMA_STREAM_CNT };
    dma_FunctionState_t state       = DMA_FUNCTION_INACTIVE;
    dma_Direction_t     direction   = DMA_DIR_PERIPH_TO_MEMORY;
    dma_TransferMode_t  mode        = DMA_TRANSFER_MODE_NORMAL;
    dma_PeriphAddr_t    periphAddr  = 0u;
    dma_MemoryAddr_t    memoryAddr  = 0u;
    dma_PeriphAddrInc_t periphInc   = DMA_PERIPH_ADDR_STATIC;
    dma_MemoryAddrInc_t memoryInc   = DMA_MEMORY_ADDR_STATIC;
    dma_TransferSize_t  size        = DMA_TRANSFER_SIZE_8BIT;
    dma_DataCount_t     dataCount   = 0u;
    dma_PeriphReqId_t   requestId   = DMA_REQ_CHANNEL_0;
    dma_Priority_t      priority    = DMA_PRIORITY_LOW;
    dma_IsrCallback     callback    = NULL;

    for( uint32_t idx = 0u; 2u > idx; idx++ )
    {
        dma_PeriphId_t  bus    = busLut[ idx ];
        dma_ChannelId_t stream = streamLut[ idx ];

        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_TransferActive( bus, stream ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_TransferInactive( bus, stream ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Get_TransferState( bus, stream, &state ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_ChannelActive( bus, stream ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_ChannelInactive( bus, stream ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Get_ChannelState( bus, stream, &state ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_Direction( bus, stream, DMA_DIR_MEMORY_TO_PERIPH ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Get_Direction( bus, stream, &direction ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_TransferMode( bus, stream, DMA_TRANSFER_MODE_CIRCULAR ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Get_TransferMode( bus, stream, &mode ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_PeriphAddr( bus, stream, TEST_DMA_PERIPH_ADDR ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Get_PeriphAddr( bus, stream, &periphAddr ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_MemoryAddr( bus, stream, TEST_DMA_MEMORY_ADDR ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Get_MemoryAddr( bus, stream, &memoryAddr ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_PeriphAddrIncrement( bus, stream, DMA_PERIPH_ADDR_INCREMENT ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Get_PeriphAddrIncrement( bus, stream, &periphInc ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_MemoryAddrIncrement( bus, stream, DMA_MEMORY_ADDR_INCREMENT ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Get_MemoryAddrIncrement( bus, stream, &memoryInc ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_PeriphTransferSize( bus, stream, DMA_TRANSFER_SIZE_16BIT ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Get_PeriphTransferSize( bus, stream, &size ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_MemoryTransferSize( bus, stream, DMA_TRANSFER_SIZE_16BIT ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Get_MemoryTransferSize( bus, stream, &size ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_DataCount( bus, stream, 1u ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Get_DataCount( bus, stream, &dataCount ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_PeripheralRequest( bus, stream, DMA_REQ_CHANNEL_1 ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Get_PeripheralRequest( bus, stream, &requestId ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_Priority( bus, stream, DMA_PRIORITY_HIGH ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Get_Priority( bus, stream, &priority ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_InterruptActive( bus, stream ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_InterruptInactive( bus, stream ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_TransferCompleteIrqActive( bus, stream ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_TransferCompleteIrqInactive( bus, stream ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_HalfTransferIrqActive( bus, stream ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_HalfTransferIrqInactive( bus, stream ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_TransferErrorIrqActive( bus, stream ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_TransferErrorIrqInactive( bus, stream ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_TransferCompleteIsrHandler( bus, stream, Ut_Dma_TransferCompleteCallback ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Get_TransferCompleteIsrHandler( bus, stream, &callback ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_HalfTransferIsrHandler( bus, stream, Ut_Dma_HalfTransferCallback ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Get_HalfTransferIsrHandler( bus, stream, &callback ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_TransferErrorIsrHandler( bus, stream, Ut_Dma_TransferErrorCallback ) );
        TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Get_TransferErrorIsrHandler( bus, stream, &callback ) );
    }

    for( uint32_t dmaBus = 0u; DMA_PERIPH_CNT > dmaBus; dmaBus++ )
    {
        for( uint32_t stream = 0u; DMA_STREAM_CNT > stream; stream++ )
        {
            TEST_ASSERT_EQUAL_HEX32( 0u, utDma_StreamRegLut[ dmaBus ][ stream ]->CR );
            TEST_ASSERT_EQUAL_HEX32( 0u, utDma_StreamRegLut[ dmaBus ][ stream ]->PAR );
            TEST_ASSERT_EQUAL_HEX32( 0u, utDma_StreamRegLut[ dmaBus ][ stream ]->M0AR );
            TEST_ASSERT_EQUAL_HEX32( 0u, utDma_StreamRegLut[ dmaBus ][ stream ]->NDTR );
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
    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Get_TransferState( DMA_PERIPH_1, DMA_STREAM_0, NULL ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Get_ChannelState( DMA_PERIPH_1, DMA_STREAM_0, NULL ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Get_Direction( DMA_PERIPH_1, DMA_STREAM_0, NULL ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Get_TransferMode( DMA_PERIPH_1, DMA_STREAM_0, NULL ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Get_PeriphAddr( DMA_PERIPH_1, DMA_STREAM_0, NULL ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Get_MemoryAddr( DMA_PERIPH_1, DMA_STREAM_0, NULL ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Get_PeriphAddrIncrement( DMA_PERIPH_1, DMA_STREAM_0, NULL ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Get_MemoryAddrIncrement( DMA_PERIPH_1, DMA_STREAM_0, NULL ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Get_PeriphTransferSize( DMA_PERIPH_1, DMA_STREAM_0, NULL ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Get_MemoryTransferSize( DMA_PERIPH_1, DMA_STREAM_0, NULL ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Get_DataCount( DMA_PERIPH_1, DMA_STREAM_0, NULL ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Get_PeripheralRequest( DMA_PERIPH_1, DMA_STREAM_0, NULL ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Get_Priority( DMA_PERIPH_1, DMA_STREAM_0, NULL ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Get_TransferCompleteIsrHandler( DMA_PERIPH_1, DMA_STREAM_0, NULL ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Get_HalfTransferIsrHandler( DMA_PERIPH_1, DMA_STREAM_0, NULL ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Get_TransferErrorIsrHandler( DMA_PERIPH_1, DMA_STREAM_0, NULL ) );
}

/* ============================== INTERRUPTS ================================ */

/**
 * \brief   Dma_Set_InterruptActive() registers stream handler and enables IRQ.
 *
 * \details DMA2 stream 7 (non contiguous NVIC number).
 *
 * \par Expected results
 * - Handler and activation of NVIC_PERIPH_IRQ_DMA2_STREAM7, DMA_REQUEST_OK.
 */
void Ut_Dma_Set_InterruptActive_RegistersHandlerAndEnablesIrq( void )
{
    TEST_ASSERT_NOT_NULL( Ut_Dma_Capture_IsrHandler( DMA_PERIPH_2, DMA_STREAM_7, NVIC_PERIPH_IRQ_DMA2_STREAM7 ) );
}


/**
 * \brief   Dma_Set_InterruptActive() reports NVIC error.
 *
 * \par Expected results
 * - DMA_REQUEST_ERROR is returned.
 */
void Ut_Dma_Set_InterruptActive_NvicError_ReturnsError( void )
{
    Nvic_Set_PeriphIrq_Handler_ExpectAndReturn( NVIC_PERIPH_IRQ_DMA1_STREAM7, NULL, NVIC_REQUEST_OK );
    Nvic_Set_PeriphIrq_Handler_IgnoreArg_irqHandler();
    Nvic_Set_PeriphIrq_Active_ExpectAndReturn( NVIC_PERIPH_IRQ_DMA1_STREAM7, NVIC_REQUEST_ERROR );

    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_InterruptActive( DMA_PERIPH_1, DMA_STREAM_7 ) );
}


/**
 * \brief   Dma_Set_InterruptInactive() disables stream IRQ.
 *
 * \par Expected results
 * - Nvic_Set_PeriphIrq_Inactive( NVIC_PERIPH_IRQ_DMA1_STREAM0 ), DMA_REQUEST_OK.
 * - NVIC error is reported as DMA_REQUEST_ERROR.
 */
void Ut_Dma_Set_InterruptInactive_DisablesIrq( void )
{
    Nvic_Set_PeriphIrq_Inactive_ExpectAndReturn( NVIC_PERIPH_IRQ_DMA1_STREAM0, NVIC_REQUEST_OK );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_InterruptInactive( DMA_PERIPH_1, DMA_STREAM_0 ) );

    Nvic_Set_PeriphIrq_Inactive_ExpectAndReturn( NVIC_PERIPH_IRQ_DMA1_STREAM0, NVIC_REQUEST_ERROR );
    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_InterruptInactive( DMA_PERIPH_1, DMA_STREAM_0 ) );
}


/**
 * \brief   Interrupt enable functions control TCIE, HTIE and TEIE.
 *
 * \par Expected results
 * - Each function sets or clears its own bit in CR only.
 */
void Ut_Dma_Set_IrqEnable_ControlsInterruptBits( void )
{
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_TransferCompleteIrqActive( DMA_PERIPH_2, DMA_STREAM_6 ) );
    TEST_ASSERT_EQUAL_HEX32( DMA_SxCR_TCIE, DMA2_Stream6->CR );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_HalfTransferIrqActive( DMA_PERIPH_2, DMA_STREAM_6 ) );
    TEST_ASSERT_EQUAL_HEX32( DMA_SxCR_TCIE | DMA_SxCR_HTIE, DMA2_Stream6->CR );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_TransferErrorIrqActive( DMA_PERIPH_2, DMA_STREAM_6 ) );
    TEST_ASSERT_EQUAL_HEX32( DMA_SxCR_TCIE | DMA_SxCR_HTIE | DMA_SxCR_TEIE, DMA2_Stream6->CR );

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_TransferCompleteIrqInactive( DMA_PERIPH_2, DMA_STREAM_6 ) );
    TEST_ASSERT_EQUAL_HEX32( DMA_SxCR_HTIE | DMA_SxCR_TEIE, DMA2_Stream6->CR );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_HalfTransferIrqInactive( DMA_PERIPH_2, DMA_STREAM_6 ) );
    TEST_ASSERT_EQUAL_HEX32( DMA_SxCR_TEIE, DMA2_Stream6->CR );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_TransferErrorIrqInactive( DMA_PERIPH_2, DMA_STREAM_6 ) );
    TEST_ASSERT_EQUAL_HEX32( 0u, DMA2_Stream6->CR );
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

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_TransferCompleteIsrHandler( DMA_PERIPH_1, DMA_STREAM_5, Ut_Dma_TransferCompleteCallback ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_HalfTransferIsrHandler( DMA_PERIPH_1, DMA_STREAM_5, Ut_Dma_HalfTransferCallback ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_TransferErrorIsrHandler( DMA_PERIPH_1, DMA_STREAM_5, Ut_Dma_TransferErrorCallback ) );

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Get_TransferCompleteIsrHandler( DMA_PERIPH_1, DMA_STREAM_5, &callback ) );
    TEST_ASSERT_EQUAL_PTR( Ut_Dma_TransferCompleteCallback, callback );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Get_HalfTransferIsrHandler( DMA_PERIPH_1, DMA_STREAM_5, &callback ) );
    TEST_ASSERT_EQUAL_PTR( Ut_Dma_HalfTransferCallback, callback );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Get_TransferErrorIsrHandler( DMA_PERIPH_1, DMA_STREAM_5, &callback ) );
    TEST_ASSERT_EQUAL_PTR( Ut_Dma_TransferErrorCallback, callback );

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_TransferCompleteIsrHandler( DMA_PERIPH_1, DMA_STREAM_5, NULL ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Get_TransferCompleteIsrHandler( DMA_PERIPH_1, DMA_STREAM_5, &callback ) );
    TEST_ASSERT_NULL( callback );
}


/**
 * \brief   Stream interrupt handler executes half transfer before transfer
 *          complete callback.
 *
 * \details DMA1 stream 1 (flag offset 6), HTIE and TCIE enabled, HTIF1 and
 *          TCIF1 active.
 *
 * \par Expected results
 * - Half transfer callback executed first, then transfer complete callback.
 * - Transfer error callback is not executed.
 * - Last write to LIFCR clears TCIF1.
 */
void Ut_Dma_Isr_HalfAndComplete_CallbacksInOrder( void )
{
    nvic_IsrCallback_t isrHandler = Ut_Dma_Capture_IsrHandler( DMA_PERIPH_1, DMA_STREAM_1, NVIC_PERIPH_IRQ_DMA1_STREAM1 );

    (void)Dma_Set_TransferCompleteIsrHandler( DMA_PERIPH_1, DMA_STREAM_1, Ut_Dma_TransferCompleteCallback );
    (void)Dma_Set_HalfTransferIsrHandler( DMA_PERIPH_1, DMA_STREAM_1, Ut_Dma_HalfTransferCallback );
    (void)Dma_Set_TransferErrorIsrHandler( DMA_PERIPH_1, DMA_STREAM_1, Ut_Dma_TransferErrorCallback );

    DMA1_Stream1->CR = DMA_SxCR_HTIE | DMA_SxCR_TCIE;
    DMA1->LISR       = DMA_LISR_HTIF1 | DMA_LISR_TCIF1;

    isrHandler();

    TEST_ASSERT_EQUAL_UINT32( 1u, utDma_HalfTransferSeq );
    TEST_ASSERT_EQUAL_UINT32( 2u, utDma_TransferCompleteSeq );
    TEST_ASSERT_EQUAL_UINT32( 0u, utDma_TransferErrorSeq );
    TEST_ASSERT_EQUAL_HEX32( DMA_LIFCR_CTCIF1, DMA1->LIFCR );
}


/**
 * \brief   Stream interrupt handler ignores flags of disabled interrupts.
 *
 * \details All flags of DMA1 stream 1 are active, no interrupt is enabled.
 *
 * \par Expected results
 * - No callback executed, no flag cleared.
 */
void Ut_Dma_Isr_InterruptsDisabled_NoCallback( void )
{
    nvic_IsrCallback_t isrHandler = Ut_Dma_Capture_IsrHandler( DMA_PERIPH_1, DMA_STREAM_1, NVIC_PERIPH_IRQ_DMA1_STREAM1 );

    (void)Dma_Set_TransferCompleteIsrHandler( DMA_PERIPH_1, DMA_STREAM_1, Ut_Dma_TransferCompleteCallback );
    (void)Dma_Set_HalfTransferIsrHandler( DMA_PERIPH_1, DMA_STREAM_1, Ut_Dma_HalfTransferCallback );
    (void)Dma_Set_TransferErrorIsrHandler( DMA_PERIPH_1, DMA_STREAM_1, Ut_Dma_TransferErrorCallback );

    DMA1->LISR = DMA_LISR_HTIF1 | DMA_LISR_TCIF1 | DMA_LISR_TEIF1;

    isrHandler();

    TEST_ASSERT_EQUAL_UINT32( 0u, utDma_CallbackCnt );
    TEST_ASSERT_EQUAL_HEX32( 0u, DMA1->LIFCR );
}


/**
 * \brief   Stream interrupt handler processes transfer error in HISR.
 *
 * \details DMA2 stream 5 (HISR, flag offset 6), TEIE enabled, TEIF5 active.
 *          Flags of other streams are active too.
 *
 * \par Expected results
 * - Transfer error callback executed only.
 * - HIFCR clears TEIF5 only.
 */
void Ut_Dma_Isr_TransferErrorHighStream_CallbackAndClear( void )
{
    nvic_IsrCallback_t isrHandler = Ut_Dma_Capture_IsrHandler( DMA_PERIPH_2, DMA_STREAM_5, NVIC_PERIPH_IRQ_DMA2_STREAM5 );

    (void)Dma_Set_TransferCompleteIsrHandler( DMA_PERIPH_2, DMA_STREAM_5, Ut_Dma_TransferCompleteCallback );
    (void)Dma_Set_TransferErrorIsrHandler( DMA_PERIPH_2, DMA_STREAM_5, Ut_Dma_TransferErrorCallback );

    DMA2_Stream5->CR = DMA_SxCR_TEIE | DMA_SxCR_TCIE;
    DMA2->HISR       = DMA_HISR_TEIF5 | DMA_HISR_TCIF4 | DMA_HISR_TCIF6;
    DMA2->LISR       = DMA_LISR_TCIF1;

    isrHandler();

    TEST_ASSERT_EQUAL_UINT32( 1u, utDma_TransferErrorSeq );
    TEST_ASSERT_EQUAL_UINT32( 0u, utDma_TransferCompleteSeq );
    TEST_ASSERT_EQUAL_HEX32( DMA_HIFCR_CTEIF5, DMA2->HIFCR );
    TEST_ASSERT_EQUAL_HEX32( 0u, DMA2->LIFCR );
}


/**
 * \brief   Stream interrupt handler clears flag without configured callback.
 *
 * \details DMA1 stream 0, TCIE enabled, TCIF0 active, callback is null.
 *
 * \par Expected results
 * - No callback executed, TCIF0 cleared.
 */
void Ut_Dma_Isr_NoCallback_ClearsFlag( void )
{
    nvic_IsrCallback_t isrHandler = Ut_Dma_Capture_IsrHandler( DMA_PERIPH_1, DMA_STREAM_0, NVIC_PERIPH_IRQ_DMA1_STREAM0 );

    (void)Dma_Set_TransferCompleteIsrHandler( DMA_PERIPH_1, DMA_STREAM_0, NULL );

    DMA1_Stream0->CR = DMA_SxCR_TCIE;
    DMA1->LISR       = DMA_LISR_TCIF0;

    isrHandler();

    TEST_ASSERT_EQUAL_UINT32( 0u, utDma_CallbackCnt );
    TEST_ASSERT_EQUAL_HEX32( DMA_LIFCR_CTCIF0, DMA1->LIFCR );
}


/**
 * \brief   Interrupt handler of every stream processes the flags of its own stream.
 *
 * \details For every stream of DMA1 and DMA2: interrupt handler registered by
 *          Dma_Set_InterruptActive() is captured, TCIE and transfer complete callback of the
 *          stream are set, TCIF of the stream is active (LISR for streams 0 - 3, HISR for
 *          streams 4 - 7, flag offsets 0 / 6 / 16 / 22), the handler is called.
 *
 * \par Expected results
 * - Transfer complete callback executed once per stream.
 * - CTCIF of the stream written to LIFCR (streams 0 - 3) or HIFCR (streams 4 - 7), the
 *   other clear register is not written.
 */
void Ut_Dma_Isr_EveryStream_OwnFlagsProcessed( void )
{
    const nvic_PeriphIrqList_t irqLut[ DMA_PERIPH_CNT ][ DMA_STREAM_CNT ] =
    {
        { NVIC_PERIPH_IRQ_DMA1_STREAM0, NVIC_PERIPH_IRQ_DMA1_STREAM1, NVIC_PERIPH_IRQ_DMA1_STREAM2, NVIC_PERIPH_IRQ_DMA1_STREAM3,
          NVIC_PERIPH_IRQ_DMA1_STREAM4, NVIC_PERIPH_IRQ_DMA1_STREAM5, NVIC_PERIPH_IRQ_DMA1_STREAM6, NVIC_PERIPH_IRQ_DMA1_STREAM7 },
        { NVIC_PERIPH_IRQ_DMA2_STREAM0, NVIC_PERIPH_IRQ_DMA2_STREAM1, NVIC_PERIPH_IRQ_DMA2_STREAM2, NVIC_PERIPH_IRQ_DMA2_STREAM3,
          NVIC_PERIPH_IRQ_DMA2_STREAM4, NVIC_PERIPH_IRQ_DMA2_STREAM5, NVIC_PERIPH_IRQ_DMA2_STREAM6, NVIC_PERIPH_IRQ_DMA2_STREAM7 },
    };
    DMA_TypeDef * const dmaRegLut[ DMA_PERIPH_CNT ] = { DMA1, DMA2 };
    const uint32_t      flagOffset[ 4u ]            = { 0u, 6u, 16u, 22u };
    const uint32_t      tcifBitPos                  = 5u;

    for( uint32_t dmaIdx = 0u; DMA_PERIPH_CNT > dmaIdx; dmaIdx++ )
    {
        for( uint32_t streamIdx = 0u; DMA_STREAM_CNT > streamIdx; streamIdx++ )
        {
            const dma_PeriphId_t  dmaBus    = (dma_PeriphId_t)dmaIdx;
            const dma_ChannelId_t dmaStream = (dma_ChannelId_t)streamIdx;
            DMA_TypeDef * const   dmaReg    = dmaRegLut[ dmaIdx ];
            const uint32_t        tcFlag    = 1uL << ( flagOffset[ streamIdx % 4u ] + tcifBitPos );

            const nvic_IsrCallback_t isrHandler = Ut_Dma_Capture_IsrHandler( dmaBus, dmaStream, irqLut[ dmaIdx ][ streamIdx ] );

            TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_TransferCompleteIsrHandler( dmaBus, dmaStream, Ut_Dma_TransferCompleteCallback ) );

            utDma_CallbackCnt         = 0u;
            utDma_TransferCompleteSeq = 0u;
            dmaReg->LISR  = ( 4u > streamIdx ) ? tcFlag : 0u;
            dmaReg->HISR  = ( 4u > streamIdx ) ? 0u : tcFlag;
            dmaReg->LIFCR = 0u;
            dmaReg->HIFCR = 0u;
            utDma_StreamRegLut[ dmaIdx ][ streamIdx ]->CR = DMA_SxCR_TCIE;

            isrHandler();

            TEST_ASSERT_EQUAL_UINT32_MESSAGE( 1u, utDma_TransferCompleteSeq, "Transfer complete callback of the stream" );
            TEST_ASSERT_EQUAL_HEX32( ( 4u > streamIdx ) ? tcFlag : 0u, dmaReg->LIFCR );
            TEST_ASSERT_EQUAL_HEX32( ( 4u > streamIdx ) ? 0u : tcFlag, dmaReg->HIFCR );
        }
    }
}

/* ========================== LOCAL FUNCTIONS =============================== */

/**
 * \brief Expects read of DMA clock state returning given state.
 *
 * \param periphId   [in]: RCC peripheral of the DMA
 * \param clockState [in]: Reported clock state
 */
static void Ut_Dma_Expect_ClockState( rcc_PeriphId_t periphId, rcc_FunctionState_t clockState )
{
    static rcc_FunctionState_t reportedState = RCC_FUNCTION_INACTIVE;

    reportedState = clockState;

    Rcc_Get_PeriphState_ExpectAndReturn( periphId, NULL, RCC_REQUEST_OK );
    Rcc_Get_PeriphState_IgnoreArg_funcState();
    Rcc_Get_PeriphState_ReturnThruPtr_funcState( &reportedState );
}


/**
 * \brief Expects activation of inactive DMA clock.
 *
 * \param periphId [in]: RCC peripheral of the DMA
 */
static void Ut_Dma_Expect_ClockActivation( rcc_PeriphId_t periphId )
{
    Ut_Dma_Expect_ClockState( periphId, RCC_FUNCTION_INACTIVE );
    Rcc_Set_PeriphActive_ExpectAndReturn( periphId, RCC_REQUEST_OK );
}


/**
 * \brief Activates interrupt of the stream and returns handler registered to NVIC.
 *
 * \param dmaBus     [in]: DMA peripheral ID
 * \param dmaChannel [in]: DMA stream ID
 * \param irqId      [in]: Expected NVIC interrupt of the stream
 * \return Interrupt handler of the stream
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
