/**
 * \author Mr.Nobody
 * \file ItTest_Dma.c
 * \ingroup Dma
 * \brief Integration tests of Direct Memory Access (DMA) module on target.
 *
 * Dma module runs on the MCU together with real RCC and NVIC modules and
 * hardware. Tests verify behavior which cannot be verified by unit tests
 * (emulated registers): clock activation, real memory to memory transfers on
 * DMA1 and DMA2, data width conversion, end of transfer, interrupts and
 * callbacks, transfer error, abort of running transfer and request routing of
 * DMAMUX1.
 *
 * All transfers are memory to memory, no board resources are used.
 *
 * \note STM32G4 channel stays enabled after the end of a normal mode transfer
 *       (only CNDTR reaches 0) - end of transfer is detected by the remaining
 *       data count. Transfer error disables the channel by hardware.
 */

/* ============================= INCLUDES =================================== */
#include "unity.h"                          /* Unity testing framework        */
#include "IntegrationTesting.h"             /* Integration testing on target  */
#include "Dma_Port.h"                       /* Module under test              */
#include "Rcc_Port.h"                       /* DMA clock state verification   */
#include "Stm32_dmamux.h"                   /* DMAMUX1 registers              */
/* ============================= TYPEDEFS =================================== */

/* ======================= FORWARD DECLARATIONS ============================= */

static void It_Dma_Get_M2mConfig            ( dma_ConfigStruct_t * const dmaConfig, const void* source, void* destination, dma_DataCount_t dataCount );
static void It_Dma_WaitTransferComplete     ( dma_PeriphId_t dmaBus );
static void It_Dma_WaitCallback             ( volatile const uint32_t * const callbackSeq );
static void It_Dma_TransferCompleteCallback ( void );
static void It_Dma_HalfTransferCallback     ( void );
static void It_Dma_TransferErrorCallback    ( void );

/* ========================= SYMBOLIC CONSTANTS ============================= */

/** DMA peripheral used by the tests */
#define IT_DMA_PERIPH                       ( DMA_PERIPH_2 )

/** DMA channel used by the tests (same channel number on DMA1) */
#define IT_DMA_CHANNEL                      ( DMA_CHANNEL_1 )

/** Count of words in transfer buffers */
#define IT_DMA_BUFFER_WORDS                 ( 64u )

/** Maximal count of polling loops until the transfer / callback is finished */
#define IT_DMA_WAIT_LOOPS                   ( 1000000u )

/**
 * Address not accessible by DMA - Cortex-M4 System Control Space on the private
 * peripheral bus is connected to the core only (DMA access ends with bus error).
 */
#define IT_DMA_INACCESSIBLE_ADDR            ( 0xE000E000u )

/* ============================== MACROS ==================================== */

/* ========================== LOCAL VARIABLES =============================== */

/** Source buffer of the transfers */
static uint32_t itDma_Source[ IT_DMA_BUFFER_WORDS ];

/** Destination buffer of the transfers */
static uint32_t itDma_Destination[ IT_DMA_BUFFER_WORDS ];

/** Count of executed callbacks (sequence counter) */
static volatile uint32_t itDma_CallbackCnt = 0u;

/** Sequence number of transfer complete callback execution (0 - not executed) */
static volatile uint32_t itDma_TransferCompleteSeq = 0u;

/** Sequence number of half transfer callback execution (0 - not executed) */
static volatile uint32_t itDma_HalfTransferSeq = 0u;

/** Sequence number of transfer error callback execution (0 - not executed) */
static volatile uint32_t itDma_TransferErrorSeq = 0u;

/* ============================= TEST SETUP ================================= */

void setUp( void )
{
    for( uint32_t idx = 0u; IT_DMA_BUFFER_WORDS > idx; idx++ )
    {
        itDma_Source[ idx ]      = 0xA5000000u + ( idx * 0x00010203u );
        itDma_Destination[ idx ] = 0u;
    }

    itDma_CallbackCnt         = 0u;
    itDma_TransferCompleteSeq = 0u;
    itDma_HalfTransferSeq     = 0u;
    itDma_TransferErrorSeq    = 0u;
}


void tearDown( void )
{
    for( uint32_t dmaIdx = 0u; DMA_PERIPH_CNT > dmaIdx; dmaIdx++ )
    {
        (void)Dma_Set_TransferInactive( (dma_PeriphId_t)dmaIdx, IT_DMA_CHANNEL );
        (void)Dma_Set_InterruptInactive( (dma_PeriphId_t)dmaIdx, IT_DMA_CHANNEL );
        (void)Dma_Set_TransferCompleteIrqInactive( (dma_PeriphId_t)dmaIdx, IT_DMA_CHANNEL );
        (void)Dma_Set_HalfTransferIrqInactive( (dma_PeriphId_t)dmaIdx, IT_DMA_CHANNEL );
        (void)Dma_Set_TransferErrorIrqInactive( (dma_PeriphId_t)dmaIdx, IT_DMA_CHANNEL );
    }
}

/* =============================== TESTS ==================================== */

/*----------------------------- Initialization -------------------------------*/

/**
 * \brief   Dma_Init() activates clocks of DMA peripheral and DMAMUX1.
 *
 * \details Initializes DMA2 channel 1 after reset (DMA2 and DMAMUX1 clock
 *          inactive) and reads RCC state of DMA2 and DMAMUX1.
 *
 * \par Expected results
 * - DMA_REQUEST_OK, DMA2 and DMAMUX1 clocks are active.
 * - Configured addresses and data count are read back.
 */
void It_Dma_Init_ClockInactive_ClocksActivatedAndConfigReadBack( void )
{
    dma_ConfigStruct_t  dmaConfig;
    rcc_FunctionState_t clockState = RCC_FUNCTION_INACTIVE;
    dma_PeriphAddr_t    periphAddr = 0u;
    dma_MemoryAddr_t    memoryAddr = 0u;
    dma_DataCount_t     dataCount  = 0u;

    It_Dma_Get_M2mConfig( &dmaConfig, itDma_Source, itDma_Destination, IT_DMA_BUFFER_WORDS );

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Init( &dmaConfig ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphState( RCC_PERIPH_DMA2, &clockState ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, clockState );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphState( RCC_PERIPH_DMAMUX1, &clockState ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, clockState );

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Get_PeriphAddr( IT_DMA_PERIPH, IT_DMA_CHANNEL, &periphAddr ) );
    TEST_ASSERT_EQUAL_HEX32( (uint32_t)itDma_Source, periphAddr );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Get_MemoryAddr( IT_DMA_PERIPH, IT_DMA_CHANNEL, &memoryAddr ) );
    TEST_ASSERT_EQUAL_HEX32( (uint32_t)itDma_Destination, memoryAddr );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Get_DataCount( IT_DMA_PERIPH, IT_DMA_CHANNEL, &dataCount ) );
    TEST_ASSERT_EQUAL_UINT32( IT_DMA_BUFFER_WORDS, dataCount );
}


/**
 * \brief   Peripheral request is routed by the DMAMUX1 channel following the DMA1 channels.
 *
 * \details USART1 TX request is set for DMA2 channel 1 and read back, DMAMUX1
 *          register is read directly.
 *
 * \note    Bug AB#1172: on 6-channel devices the module wrote the request into the not
 *          implemented slot DMAMUX1_BASE + 0x18 - the register is taken from the CMSIS
 *          header (DMAMUX1_Channel6 = DMAMUX1_BASE + 0x20 there, DMAMUX1_Channel8 on
 *          8-channel devices).
 *
 * \par Expected results
 * - DMA_REQUEST_OK, request read back.
 * - DMAREQ_ID of the CMSIS DMAMUX1 channel of DMA2 channel 1 holds USART1 TX.
 */
void It_Dma_Set_PeripheralRequest_Dma2Channel1_DmamuxChannelAfterDma1( void )
{
    dma_ConfigStruct_t             dmaConfig;
    dma_PeriphReqId_t              requestId = DMA_REQ_MEM2MEM;
#if defined(DMA1_Channel8)
    DMAMUX_Channel_TypeDef * const muxReg    = DMAMUX1_Channel8;
#else
    DMAMUX_Channel_TypeDef * const muxReg    = DMAMUX1_Channel6;
#endif

    It_Dma_Get_M2mConfig( &dmaConfig, itDma_Source, itDma_Destination, IT_DMA_BUFFER_WORDS );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Init( &dmaConfig ) );

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_PeripheralRequest( IT_DMA_PERIPH, IT_DMA_CHANNEL, DMA_REQ_USART1_TX ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Get_PeripheralRequest( IT_DMA_PERIPH, IT_DMA_CHANNEL, &requestId ) );
    TEST_ASSERT_EQUAL( DMA_REQ_USART1_TX, requestId );

    TEST_ASSERT_EQUAL_HEX32( LL_DMAMUX_REQ_USART1_TX, muxReg->CCR & DMAMUX_CxCR_DMAREQ_ID );

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_PeripheralRequest( IT_DMA_PERIPH, IT_DMA_CHANNEL, DMA_REQ_MEM2MEM ) );
}

/*------------------------------ Data transfers ------------------------------*/

/**
 * \brief   Memory to memory transfer of words on DMA2 copies the buffer.
 *
 * \details 64 words are transferred by polling, end of transfer is detected
 *          by remaining data count 0.
 *
 * \par Expected results
 * - Destination equals source, remaining data count is 0.
 */
void It_Dma_Transfer_MemToMemWords_BufferCopied( void )
{
    dma_ConfigStruct_t dmaConfig;

    It_Dma_Get_M2mConfig( &dmaConfig, itDma_Source, itDma_Destination, IT_DMA_BUFFER_WORDS );

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Init( &dmaConfig ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_TransferActive( IT_DMA_PERIPH, IT_DMA_CHANNEL ) );

    It_Dma_WaitTransferComplete( IT_DMA_PERIPH );

    TEST_ASSERT_EQUAL_HEX32_ARRAY( itDma_Source, itDma_Destination, IT_DMA_BUFFER_WORDS );
}


/**
 * \brief   Memory to memory transfer works on DMA1 as well.
 *
 * \details STM32G4 DMA1 supports memory to memory direction. 64 words on DMA1
 *          channel 1.
 *
 * \par Expected results
 * - Destination equals source, remaining data count is 0.
 */
void It_Dma_Transfer_Dma1MemToMem_BufferCopied( void )
{
    dma_ConfigStruct_t dmaConfig;

    It_Dma_Get_M2mConfig( &dmaConfig, itDma_Source, itDma_Destination, IT_DMA_BUFFER_WORDS );
    dmaConfig.DmaPeriphId = DMA_PERIPH_1;

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Init( &dmaConfig ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_TransferActive( DMA_PERIPH_1, IT_DMA_CHANNEL ) );

    It_Dma_WaitTransferComplete( DMA_PERIPH_1 );

    TEST_ASSERT_EQUAL_HEX32_ARRAY( itDma_Source, itDma_Destination, IT_DMA_BUFFER_WORDS );
}


/**
 * \brief   Bytes are zero extended to words.
 *
 * \details Source 8 bit, destination 32 bit, 32 data (count in source units).
 *          STM32G4 DMA has no FIFO - every byte is written as one word.
 *
 * \par Expected results
 * - Destination word n holds source byte n (upper bytes 0).
 * - Word behind the transferred data is not written.
 */
void It_Dma_Transfer_BytesToWords_ZeroExtended( void )
{
    dma_ConfigStruct_t   dmaConfig;
    const uint32_t       byteCnt     = IT_DMA_BUFFER_WORDS / 2u;
    const uint8_t* const sourceBytes = (const uint8_t*)itDma_Source;

    It_Dma_Get_M2mConfig( &dmaConfig, itDma_Source, itDma_Destination, byteCnt );
    dmaConfig.PeriphTransferSize = DMA_TRANSFER_SIZE_8BIT;
    dmaConfig.MemoryTransferSize = DMA_TRANSFER_SIZE_32BIT;

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Init( &dmaConfig ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_TransferActive( IT_DMA_PERIPH, IT_DMA_CHANNEL ) );

    It_Dma_WaitTransferComplete( IT_DMA_PERIPH );

    for( uint32_t idx = 0u; byteCnt > idx; idx++ )
    {
        TEST_ASSERT_EQUAL_HEX32( (uint32_t)sourceBytes[ idx ], itDma_Destination[ idx ] );
    }

    TEST_ASSERT_EQUAL_HEX32( 0u, itDma_Destination[ byteCnt ] );
}


/**
 * \brief   Static source address fills destination with one value.
 *
 * \details Source address is not incremented, 16 bit transfers.
 *
 * \par Expected results
 * - All transferred half words of destination equal the first source half word.
 */
void It_Dma_Transfer_StaticSource_DestinationFilled( void )
{
    dma_ConfigStruct_t dmaConfig;
    const uint16_t*    source      = (const uint16_t*)itDma_Source;
    const uint16_t*    destination = (const uint16_t*)itDma_Destination;

    It_Dma_Get_M2mConfig( &dmaConfig, itDma_Source, itDma_Destination, IT_DMA_BUFFER_WORDS );
    dmaConfig.PeriphAddrIncrement = DMA_PERIPH_ADDR_STATIC;
    dmaConfig.PeriphTransferSize  = DMA_TRANSFER_SIZE_16BIT;
    dmaConfig.MemoryTransferSize  = DMA_TRANSFER_SIZE_16BIT;

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Init( &dmaConfig ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_TransferActive( IT_DMA_PERIPH, IT_DMA_CHANNEL ) );

    It_Dma_WaitTransferComplete( IT_DMA_PERIPH );

    for( uint32_t idx = 0u; IT_DMA_BUFFER_WORDS > idx; idx++ )
    {
        TEST_ASSERT_EQUAL_HEX16( source[ 0u ], destination[ idx ] );
    }

    TEST_ASSERT_EQUAL_HEX16( 0u, destination[ IT_DMA_BUFFER_WORDS ] );
}


/**
 * \brief   Circular mode is refused in memory to memory direction.
 *
 * \details Memory to memory configuration with circular mode.
 *
 * \par Expected results
 * - Dma_Init() DMA_REQUEST_OK, Dma_Set_TransferActive() DMA_REQUEST_ERROR.
 * - Channel stays disabled, destination is not written.
 */
void It_Dma_Set_TransferActive_MemToMemCircular_Refused( void )
{
    dma_ConfigStruct_t  dmaConfig;
    dma_FunctionState_t state = DMA_FUNCTION_ACTIVE;

    It_Dma_Get_M2mConfig( &dmaConfig, itDma_Source, itDma_Destination, IT_DMA_BUFFER_WORDS );
    dmaConfig.TransferMode = DMA_TRANSFER_MODE_CIRCULAR;

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Init( &dmaConfig ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_TransferActive( IT_DMA_PERIPH, IT_DMA_CHANNEL ) );

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Get_TransferState( IT_DMA_PERIPH, IT_DMA_CHANNEL, &state ) );
    TEST_ASSERT_EQUAL( DMA_FUNCTION_INACTIVE, state );
    TEST_ASSERT_EQUAL_HEX32( 0u, itDma_Destination[ 0u ] );
}

/*-------------------------------- Interrupts --------------------------------*/

/**
 * \brief   Transfer complete and half transfer interrupts execute callbacks.
 *
 * \details Channel interrupt is activated in NVIC, half transfer and transfer
 *          complete interrupts are enabled on the channel.
 *
 * \par Expected results
 * - Half transfer callback is executed before transfer complete callback.
 * - Transfer error callback is not executed, data are copied.
 */
void It_Dma_Interrupts_HalfAndComplete_CallbacksInOrder( void )
{
    dma_ConfigStruct_t dmaConfig;

    It_Dma_Get_M2mConfig( &dmaConfig, itDma_Source, itDma_Destination, IT_DMA_BUFFER_WORDS );
    dmaConfig.TransferCompleteCallback = It_Dma_TransferCompleteCallback;
    dmaConfig.HalfTransferCallback     = It_Dma_HalfTransferCallback;
    dmaConfig.TransferErrorCallback    = It_Dma_TransferErrorCallback;

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Init( &dmaConfig ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_InterruptActive( IT_DMA_PERIPH, IT_DMA_CHANNEL ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_HalfTransferIrqActive( IT_DMA_PERIPH, IT_DMA_CHANNEL ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_TransferCompleteIrqActive( IT_DMA_PERIPH, IT_DMA_CHANNEL ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_TransferErrorIrqActive( IT_DMA_PERIPH, IT_DMA_CHANNEL ) );

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_TransferActive( IT_DMA_PERIPH, IT_DMA_CHANNEL ) );

    It_Dma_WaitCallback( &itDma_TransferCompleteSeq );

    TEST_ASSERT_NOT_EQUAL( 0u, itDma_HalfTransferSeq );
    TEST_ASSERT_LESS_THAN_UINT32( itDma_TransferCompleteSeq, itDma_HalfTransferSeq );
    TEST_ASSERT_EQUAL_UINT32( 0u, itDma_TransferErrorSeq );
    TEST_ASSERT_EQUAL_HEX32_ARRAY( itDma_Source, itDma_Destination, IT_DMA_BUFFER_WORDS );
}


/**
 * \brief   Transfer from address not accessible by DMA is reported as error.
 *
 * \details Source is the System Control Space (private peripheral bus of the
 *          core). Interrupts are not enabled, transfer error flag stays active.
 *
 * \par Expected results
 * - Dma_Set_TransferActive() returns DMA_REQUEST_ERROR.
 * - Channel is disabled by hardware, destination is not written.
 */
void It_Dma_TransferError_InaccessibleSource_ActivationFails( void )
{
    dma_ConfigStruct_t  dmaConfig;
    dma_FunctionState_t state = DMA_FUNCTION_ACTIVE;

    It_Dma_Get_M2mConfig( &dmaConfig, (const void*)IT_DMA_INACCESSIBLE_ADDR, itDma_Destination, IT_DMA_BUFFER_WORDS );

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Init( &dmaConfig ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_TransferActive( IT_DMA_PERIPH, IT_DMA_CHANNEL ) );

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Get_TransferState( IT_DMA_PERIPH, IT_DMA_CHANNEL, &state ) );
    TEST_ASSERT_EQUAL( DMA_FUNCTION_INACTIVE, state );
    TEST_ASSERT_EQUAL_HEX32( 0u, itDma_Destination[ 0u ] );
}


/**
 * \brief   Transfer error interrupt executes transfer error callback.
 *
 * \details Source is the System Control Space (not accessible by DMA), transfer
 *          error and transfer complete interrupts are enabled.
 *
 * \note    Return value of Dma_Set_TransferActive() is not checked - interrupt
 *          handler may clear transfer error flag before activation is verified.
 *
 * \par Expected results
 * - Transfer error callback is executed, transfer complete callback is not.
 */
void It_Dma_TransferError_Interrupt_ErrorCallback( void )
{
    dma_ConfigStruct_t dmaConfig;

    It_Dma_Get_M2mConfig( &dmaConfig, (const void*)IT_DMA_INACCESSIBLE_ADDR, itDma_Destination, IT_DMA_BUFFER_WORDS );
    dmaConfig.TransferCompleteCallback = It_Dma_TransferCompleteCallback;
    dmaConfig.TransferErrorCallback    = It_Dma_TransferErrorCallback;

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Init( &dmaConfig ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_InterruptActive( IT_DMA_PERIPH, IT_DMA_CHANNEL ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_TransferCompleteIrqActive( IT_DMA_PERIPH, IT_DMA_CHANNEL ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_TransferErrorIrqActive( IT_DMA_PERIPH, IT_DMA_CHANNEL ) );

    (void)Dma_Set_TransferActive( IT_DMA_PERIPH, IT_DMA_CHANNEL );

    It_Dma_WaitCallback( &itDma_TransferErrorSeq );

    TEST_ASSERT_EQUAL_UINT32( 0u, itDma_TransferCompleteSeq );
}

/*--------------------------- Running transfer -------------------------------*/

/**
 * \brief   Running transfer is aborted without transfer complete callback.
 *
 * \details Long transfer (65535 words, static addresses) with transfer complete
 *          interrupt is started and immediately deactivated.
 *
 * \par Expected results
 * - Dma_Set_TransferInactive() returns DMA_REQUEST_OK, channel is disabled.
 * - Remaining data count is not 0 (transfer aborted).
 * - Transfer complete callback is not executed.
 */
void It_Dma_Set_TransferInactive_RunningTransfer_AbortedWithoutCallback( void )
{
    dma_ConfigStruct_t  dmaConfig;
    dma_FunctionState_t state     = DMA_FUNCTION_ACTIVE;
    dma_DataCount_t     dataCount = 0u;

    It_Dma_Get_M2mConfig( &dmaConfig, itDma_Source, itDma_Destination, DMA_DATA_COUNT_MAX );
    dmaConfig.PeriphAddrIncrement      = DMA_PERIPH_ADDR_STATIC;
    dmaConfig.MemoryAddrIncrement      = DMA_MEMORY_ADDR_STATIC;
    dmaConfig.TransferCompleteCallback = It_Dma_TransferCompleteCallback;

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Init( &dmaConfig ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_InterruptActive( IT_DMA_PERIPH, IT_DMA_CHANNEL ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_TransferCompleteIrqActive( IT_DMA_PERIPH, IT_DMA_CHANNEL ) );

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_TransferActive( IT_DMA_PERIPH, IT_DMA_CHANNEL ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_TransferInactive( IT_DMA_PERIPH, IT_DMA_CHANNEL ) );

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Get_TransferState( IT_DMA_PERIPH, IT_DMA_CHANNEL, &state ) );
    TEST_ASSERT_EQUAL( DMA_FUNCTION_INACTIVE, state );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Get_DataCount( IT_DMA_PERIPH, IT_DMA_CHANNEL, &dataCount ) );
    TEST_ASSERT_NOT_EQUAL( 0u, dataCount );
    TEST_ASSERT_EQUAL_UINT32( 0u, itDma_TransferCompleteSeq );
}


/**
 * \brief   Configuration of running channel is rejected.
 *
 * \details Long transfer (65535 words, static addresses) is started, then
 *          addresses and data count are written.
 *
 * \par Expected results
 * - Setters return DMA_REQUEST_ERROR, configuration is not changed.
 */
void It_Dma_Set_Configuration_RunningTransfer_Rejected( void )
{
    dma_ConfigStruct_t dmaConfig;
    dma_PeriphAddr_t   periphAddr = 0u;

    It_Dma_Get_M2mConfig( &dmaConfig, itDma_Source, itDma_Destination, DMA_DATA_COUNT_MAX );
    dmaConfig.PeriphAddrIncrement = DMA_PERIPH_ADDR_STATIC;
    dmaConfig.MemoryAddrIncrement = DMA_MEMORY_ADDR_STATIC;

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Init( &dmaConfig ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_TransferActive( IT_DMA_PERIPH, IT_DMA_CHANNEL ) );

    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_PeriphAddr( IT_DMA_PERIPH, IT_DMA_CHANNEL, (uint32_t)itDma_Destination ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_DataCount( IT_DMA_PERIPH, IT_DMA_CHANNEL, 1u ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_Priority( IT_DMA_PERIPH, IT_DMA_CHANNEL, DMA_PRIORITY_LOW ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_PeripheralRequest( IT_DMA_PERIPH, IT_DMA_CHANNEL, DMA_REQ_USART1_TX ) );

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Get_PeriphAddr( IT_DMA_PERIPH, IT_DMA_CHANNEL, &periphAddr ) );
    TEST_ASSERT_EQUAL_HEX32( (uint32_t)itDma_Source, periphAddr );

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_TransferInactive( IT_DMA_PERIPH, IT_DMA_CHANNEL ) );
}

/* ========================== LOCAL FUNCTIONS =============================== */

/**
 * \brief Fills configuration of memory to memory word transfer of test channel.
 *
 * \param dmaConfig   [out]: DMA configuration
 * \param source       [in]: Source address
 * \param destination  [in]: Destination address
 * \param dataCount    [in]: Count of transferred data (source units)
 */
static void It_Dma_Get_M2mConfig( dma_ConfigStruct_t * const dmaConfig, const void* source, void* destination, dma_DataCount_t dataCount )
{
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Get_DefaultConfig( dmaConfig ) );

    dmaConfig->DmaPeriphId         = IT_DMA_PERIPH;
    dmaConfig->DmaChannel          = IT_DMA_CHANNEL;
    dmaConfig->Direction           = DMA_DIR_MEMORY_TO_MEMORY;
    dmaConfig->PeriphAddress       = (dma_PeriphAddr_t)source;
    dmaConfig->MemoryAddress       = (dma_MemoryAddr_t)destination;
    dmaConfig->PeriphAddrIncrement = DMA_PERIPH_ADDR_INCREMENT;
    dmaConfig->MemoryAddrIncrement = DMA_MEMORY_ADDR_INCREMENT;
    dmaConfig->PeriphTransferSize  = DMA_TRANSFER_SIZE_32BIT;
    dmaConfig->MemoryTransferSize  = DMA_TRANSFER_SIZE_32BIT;
    dmaConfig->DataCount           = dataCount;
    dmaConfig->PeripheralReqId     = DMA_REQ_MEM2MEM;
    dmaConfig->Priority            = DMA_PRIORITY_HIGH;
}


/**
 * \brief Waits until all data of the test channel are transferred (remaining
 *        data count 0 - STM32G4 channel stays enabled in normal mode).
 *
 * \param dmaBus [in]: DMA peripheral of the test channel
 */
static void It_Dma_WaitTransferComplete( dma_PeriphId_t dmaBus )
{
    dma_DataCount_t dataCount = 1u;
    uint32_t        loopCnt   = 0u;

    while( ( 0u                 != dataCount ) &&
           ( IT_DMA_WAIT_LOOPS  >  loopCnt   )    )
    {
        TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Get_DataCount( dmaBus, IT_DMA_CHANNEL, &dataCount ) );
        loopCnt++;
    }

    TEST_ASSERT_EQUAL_UINT32_MESSAGE( 0u, dataCount, "Transfer not finished" );
}


/**
 * \brief Waits until the callback is executed.
 *
 * \param callbackSeq [in]: Sequence number of the callback (0 - not executed)
 */
static void It_Dma_WaitCallback( volatile const uint32_t * const callbackSeq )
{
    uint32_t loopCnt = 0u;

    while( ( 0u                == *callbackSeq ) &&
           ( IT_DMA_WAIT_LOOPS  >  loopCnt     )    )
    {
        /* Waiting for interrupt */
        loopCnt++;
    }

    TEST_ASSERT_NOT_EQUAL_MESSAGE( 0u, *callbackSeq, "Callback not executed" );
}


/** \brief Transfer complete callback recording execution sequence */
static void It_Dma_TransferCompleteCallback( void )
{
    itDma_TransferCompleteSeq = ++itDma_CallbackCnt;
}


/** \brief Half transfer callback recording execution sequence */
static void It_Dma_HalfTransferCallback( void )
{
    itDma_HalfTransferSeq = ++itDma_CallbackCnt;
}


/** \brief Transfer error callback recording execution sequence */
static void It_Dma_TransferErrorCallback( void )
{
    itDma_TransferErrorSeq = ++itDma_CallbackCnt;
}
