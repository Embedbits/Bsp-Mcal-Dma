/**
 * \author Mr.Nobody
 * \file ItTest_Dma.c
 * \ingroup Dma
 * \brief Integration tests of Direct Memory Access (DMA) module on target.
 *
 * Dma module runs on the MCU together with real RCC and NVIC modules and
 * hardware. Tests verify behavior which cannot be verified by unit tests
 * (emulated registers): clock activation, real memory to memory transfers,
 * data packing through FIFO, end of transfer by hardware, interrupts and
 * callbacks, transfer error and abort of running transfer.
 *
 * All transfers are memory to memory on DMA2 (DMA1 does not support memory to
 * memory direction), no board resources are used. Buffers are placed in RAM of
 * the linker script (DTCM / SRAM1 - both accessible by DMA, D-Cache is disabled
 * by Nvic_Init so no cache maintenance is needed).
 */

/* ============================= INCLUDES =================================== */
#include "unity.h"                          /* Unity testing framework        */
#include "IntegrationTesting.h"             /* Integration testing on target  */
#include "Dma_Port.h"                       /* Module under test              */
#include "Rcc_Port.h"                       /* DMA clock state verification   */
/* ============================= TYPEDEFS =================================== */

/* ======================= FORWARD DECLARATIONS ============================= */

static void It_Dma_Get_M2mConfig            ( dma_ConfigStruct_t * const dmaConfig, const void* source, void* destination, dma_DataCount_t dataCount );
static void It_Dma_WaitTransferInactive     ( void );
static void It_Dma_WaitCallback             ( volatile const uint32_t * const callbackSeq );
static void It_Dma_TransferCompleteCallback ( void );
static void It_Dma_HalfTransferCallback     ( void );
static void It_Dma_TransferErrorCallback    ( void );

/* ========================= SYMBOLIC CONSTANTS ============================= */

/** DMA peripheral used by the tests (memory to memory capable) */
#define IT_DMA_PERIPH                       ( DMA_PERIPH_2 )

/** DMA stream used by the tests */
#define IT_DMA_STREAM                       ( DMA_STREAM_0 )

/** Count of words in transfer buffers */
#define IT_DMA_BUFFER_WORDS                 ( 64u )

/** Maximal count of polling loops until the transfer / callback is finished */
#define IT_DMA_WAIT_LOOPS                   ( 1000000u )

/**
 * Address not accessible by DMA - reserved region of STM32F7 memory map (no CCM
 * RAM at 0x10000000), the DMA access ends with bus error.
 */
#define IT_DMA_INACCESSIBLE_ADDR            ( 0x10000000u )

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
    (void)Dma_Set_TransferInactive( IT_DMA_PERIPH, IT_DMA_STREAM );
    (void)Dma_Set_InterruptInactive( IT_DMA_PERIPH, IT_DMA_STREAM );
    (void)Dma_Set_TransferCompleteIrqInactive( IT_DMA_PERIPH, IT_DMA_STREAM );
    (void)Dma_Set_HalfTransferIrqInactive( IT_DMA_PERIPH, IT_DMA_STREAM );
    (void)Dma_Set_TransferErrorIrqInactive( IT_DMA_PERIPH, IT_DMA_STREAM );
}

/* =============================== TESTS ==================================== */

/*----------------------------- Initialization -------------------------------*/

/**
 * \brief   Dma_Init() activates clock of DMA peripheral.
 *
 * \details Initializes DMA2 stream after reset (DMA2 clock inactive) and reads
 *          RCC state of DMA2.
 *
 * \par Expected results
 * - DMA_REQUEST_OK, DMA2 clock is active.
 * - Configured addresses and data count are read back.
 */
void It_Dma_Init_ClockInactive_ClockActivatedAndConfigReadBack( void )
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

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Get_PeriphAddr( IT_DMA_PERIPH, IT_DMA_STREAM, &periphAddr ) );
    TEST_ASSERT_EQUAL_HEX32( (uint32_t)itDma_Source, periphAddr );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Get_MemoryAddr( IT_DMA_PERIPH, IT_DMA_STREAM, &memoryAddr ) );
    TEST_ASSERT_EQUAL_HEX32( (uint32_t)itDma_Destination, memoryAddr );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Get_DataCount( IT_DMA_PERIPH, IT_DMA_STREAM, &dataCount ) );
    TEST_ASSERT_EQUAL_UINT32( IT_DMA_BUFFER_WORDS, dataCount );
}

/*------------------------------ Data transfers ------------------------------*/

/**
 * \brief   Memory to memory transfer of words copies the buffer.
 *
 * \details 64 words are transferred by polling, end of transfer is detected
 *          by stream disabled by hardware.
 *
 * \par Expected results
 * - Destination equals source, remaining data count is 0.
 */
void It_Dma_Transfer_MemToMemWords_BufferCopied( void )
{
    dma_ConfigStruct_t dmaConfig;
    dma_DataCount_t    dataCount = 1u;

    It_Dma_Get_M2mConfig( &dmaConfig, itDma_Source, itDma_Destination, IT_DMA_BUFFER_WORDS );

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Init( &dmaConfig ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_TransferActive( IT_DMA_PERIPH, IT_DMA_STREAM ) );

    It_Dma_WaitTransferInactive();

    TEST_ASSERT_EQUAL_HEX32_ARRAY( itDma_Source, itDma_Destination, IT_DMA_BUFFER_WORDS );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Get_DataCount( IT_DMA_PERIPH, IT_DMA_STREAM, &dataCount ) );
    TEST_ASSERT_EQUAL_UINT32( 0u, dataCount );
}


/**
 * \brief   Bytes are packed to words through FIFO.
 *
 * \details Source 8 bit, destination 32 bit - Dma_Init() enables FIFO. Data
 *          count is in source units (bytes).
 *
 * \par Expected results
 * - Destination words hold source bytes (little endian packing).
 * - Word behind the transferred data is not written.
 */
void It_Dma_Transfer_BytesToWords_PackedByFifo( void )
{
    dma_ConfigStruct_t dmaConfig;
    const uint32_t     wordCnt = IT_DMA_BUFFER_WORDS / 2u;

    It_Dma_Get_M2mConfig( &dmaConfig, itDma_Source, itDma_Destination, wordCnt * sizeof( uint32_t ) );
    dmaConfig.PeriphTransferSize = DMA_TRANSFER_SIZE_8BIT;
    dmaConfig.MemoryTransferSize = DMA_TRANSFER_SIZE_32BIT;

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Init( &dmaConfig ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_TransferActive( IT_DMA_PERIPH, IT_DMA_STREAM ) );

    It_Dma_WaitTransferInactive();

    TEST_ASSERT_EQUAL_HEX32_ARRAY( itDma_Source, itDma_Destination, wordCnt );
    TEST_ASSERT_EQUAL_HEX32( 0u, itDma_Destination[ wordCnt ] );
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
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_TransferActive( IT_DMA_PERIPH, IT_DMA_STREAM ) );

    It_Dma_WaitTransferInactive();

    for( uint32_t idx = 0u; IT_DMA_BUFFER_WORDS > idx; idx++ )
    {
        TEST_ASSERT_EQUAL_HEX16( source[ 0u ], destination[ idx ] );
    }

    TEST_ASSERT_EQUAL_HEX16( 0u, destination[ IT_DMA_BUFFER_WORDS ] );
}

/*-------------------------------- Interrupts --------------------------------*/

/**
 * \brief   Transfer complete and half transfer interrupts execute callbacks.
 *
 * \details Stream interrupt is activated in NVIC, half transfer and transfer
 *          complete interrupts are enabled on the stream.
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
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_InterruptActive( IT_DMA_PERIPH, IT_DMA_STREAM ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_HalfTransferIrqActive( IT_DMA_PERIPH, IT_DMA_STREAM ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_TransferCompleteIrqActive( IT_DMA_PERIPH, IT_DMA_STREAM ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_TransferErrorIrqActive( IT_DMA_PERIPH, IT_DMA_STREAM ) );

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_TransferActive( IT_DMA_PERIPH, IT_DMA_STREAM ) );

    It_Dma_WaitCallback( &itDma_TransferCompleteSeq );

    TEST_ASSERT_NOT_EQUAL( 0u, itDma_HalfTransferSeq );
    TEST_ASSERT_LESS_THAN_UINT32( itDma_TransferCompleteSeq, itDma_HalfTransferSeq );
    TEST_ASSERT_EQUAL_UINT32( 0u, itDma_TransferErrorSeq );
    TEST_ASSERT_EQUAL_HEX32_ARRAY( itDma_Source, itDma_Destination, IT_DMA_BUFFER_WORDS );
}


/**
 * \brief   Transfer from address not accessible by DMA is reported as error.
 *
 * \details Source is reserved address region (bus error). Interrupts are not
 *          enabled, transfer error flag stays active.
 *
 * \par Expected results
 * - Dma_Set_TransferActive() returns DMA_REQUEST_ERROR.
 * - Stream is disabled by hardware, destination is not written.
 */
void It_Dma_TransferError_InaccessibleSource_ActivationFails( void )
{
    dma_ConfigStruct_t  dmaConfig;
    dma_FunctionState_t state = DMA_FUNCTION_ACTIVE;

    It_Dma_Get_M2mConfig( &dmaConfig, (const void*)IT_DMA_INACCESSIBLE_ADDR, itDma_Destination, IT_DMA_BUFFER_WORDS );

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Init( &dmaConfig ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_TransferActive( IT_DMA_PERIPH, IT_DMA_STREAM ) );

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Get_TransferState( IT_DMA_PERIPH, IT_DMA_STREAM, &state ) );
    TEST_ASSERT_EQUAL( DMA_FUNCTION_INACTIVE, state );
    TEST_ASSERT_EQUAL_HEX32( 0u, itDma_Destination[ 0u ] );
}


/**
 * \brief   Transfer error interrupt executes transfer error callback.
 *
 * \details Source is reserved address region (bus error), transfer error and
 *          transfer complete interrupts are enabled.
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
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_InterruptActive( IT_DMA_PERIPH, IT_DMA_STREAM ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_TransferCompleteIrqActive( IT_DMA_PERIPH, IT_DMA_STREAM ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_TransferErrorIrqActive( IT_DMA_PERIPH, IT_DMA_STREAM ) );

    (void)Dma_Set_TransferActive( IT_DMA_PERIPH, IT_DMA_STREAM );

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
 * - Dma_Set_TransferInactive() returns DMA_REQUEST_OK, stream is disabled.
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
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_InterruptActive( IT_DMA_PERIPH, IT_DMA_STREAM ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_TransferCompleteIrqActive( IT_DMA_PERIPH, IT_DMA_STREAM ) );

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_TransferActive( IT_DMA_PERIPH, IT_DMA_STREAM ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_TransferInactive( IT_DMA_PERIPH, IT_DMA_STREAM ) );

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Get_TransferState( IT_DMA_PERIPH, IT_DMA_STREAM, &state ) );
    TEST_ASSERT_EQUAL( DMA_FUNCTION_INACTIVE, state );
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Get_DataCount( IT_DMA_PERIPH, IT_DMA_STREAM, &dataCount ) );
    TEST_ASSERT_NOT_EQUAL( 0u, dataCount );
    TEST_ASSERT_EQUAL_UINT32( 0u, itDma_TransferCompleteSeq );
}


/**
 * \brief   Configuration of running stream is rejected.
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
    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_TransferActive( IT_DMA_PERIPH, IT_DMA_STREAM ) );

    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_PeriphAddr( IT_DMA_PERIPH, IT_DMA_STREAM, (uint32_t)itDma_Destination ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_DataCount( IT_DMA_PERIPH, IT_DMA_STREAM, 1u ) );
    TEST_ASSERT_EQUAL( DMA_REQUEST_ERROR, Dma_Set_Priority( IT_DMA_PERIPH, IT_DMA_STREAM, DMA_PRIORITY_LOW ) );

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Get_PeriphAddr( IT_DMA_PERIPH, IT_DMA_STREAM, &periphAddr ) );
    TEST_ASSERT_EQUAL_HEX32( (uint32_t)itDma_Source, periphAddr );

    TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Set_TransferInactive( IT_DMA_PERIPH, IT_DMA_STREAM ) );
}

/* ========================== LOCAL FUNCTIONS =============================== */

/**
 * \brief Fills configuration of memory to memory word transfer of test stream.
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
    dmaConfig->DmaChannel          = IT_DMA_STREAM;
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
 * \brief Waits until the test stream is disabled by hardware (end of transfer).
 */
static void It_Dma_WaitTransferInactive( void )
{
    dma_FunctionState_t state = DMA_FUNCTION_ACTIVE;

    for( uint32_t loopCnt = 0u;
         ( DMA_FUNCTION_INACTIVE != state ) &&
         ( IT_DMA_WAIT_LOOPS > loopCnt );
         loopCnt++ )
    {
        TEST_ASSERT_EQUAL( DMA_REQUEST_OK, Dma_Get_TransferState( IT_DMA_PERIPH, IT_DMA_STREAM, &state ) );
    }

    TEST_ASSERT_EQUAL_MESSAGE( DMA_FUNCTION_INACTIVE, state, "Transfer not finished" );
}


/**
 * \brief Waits until the callback is executed.
 *
 * \param callbackSeq [in]: Sequence number of the callback (0 - not executed)
 */
static void It_Dma_WaitCallback( volatile const uint32_t * const callbackSeq )
{
    for( uint32_t loopCnt = 0u;
         ( 0u == *callbackSeq ) &&
         ( IT_DMA_WAIT_LOOPS > loopCnt );
         loopCnt++ )
    {
        /* Waiting for interrupt */
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
