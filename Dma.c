/**
 * \author Mr.Nobody
 * \file Dma.c
 * \ingroup Dma
 * \brief Dma module common functionality
 *
 * STM32H7 Direct Memory Access (DMA) controller has 8 streams per DMA
 * peripheral (DMA1, DMA2). Transfer "channel" of the module interface
 * (\ref dma_ChannelId_t) represents DMA stream, peripheral request
 * (\ref dma_PeriphReqId_t) is routed to the stream by DMAMUX1 (channel of DMAMUX1
 * = stream of DMA1, stream + 8 of DMA2). Memory to memory transfer is available
 * on both DMA peripherals.
 *
 * \note Configuration registers of the stream are write protected by hardware
 *       while the stream is enabled. All configuration functions return error
 *       if the stream is enabled.
 *
 * \note Device errata "UART / USART - DMA transfer lock" (STM32H7, applied by ST
 *       HAL): stream serving UART / USART request uses bufferable transfers
 *       (SxCR TRBUFF), set by \ref Dma_Set_PeripheralRequest (not on revision Y
 *       of STM32H742 / H743 / H745 / H747 / H750 / H753 / H755 / H757).
 *
 * \note  The module is empty on MCUs without DMA1 / DMA2 streams (STM32H7R / STM32H7S -
 *        GPDMA1 / HPDMA1 are handled by the Gpdma module).
 */
/* ============================== INCLUDES ================================== */
#include "Dma.h"                            /* Self include                   */
#include "Dma_Port.h"                       /* Own port file include          */
#include "Dma_Types.h"                      /* Module types definitions       */
#include "Stm32_dma.h"                      /* DMA Registers definitions      */
#include "Rcc_Port.h"                       /* RCC module functionality       */
#include "Nvic_Port.h"                      /* NVIC module functionality      */

#if defined(DMA1)
/* ========================== SYMBOLIC CONSTANTS ============================ */

/** Value of major version of SW module */
#define DMA_MAJOR_VERSION           ( 1u )

/** Value of minor version of SW module */
#define DMA_MINOR_VERSION           ( 0u )

/** Value of patch version of SW module */
#define DMA_PATCH_VERSION           ( 0u )

/** Maximum timeout used for waiting for register update verification */
#define DMA_TIMEOUT_RAW             ( 0x84FCB )

/** Count of streams in one interrupt status register (LISR - streams 0-3, HISR - streams 4-7) */
#define DMA_ISR_STREAM_CNT          ( 4u )

/** FIFO error flag of the stream (relative to stream offset in LISR / HISR) */
#define DMA_FLAG_FE                 ( DMA_LISR_FEIF0 )

/** Direct mode error flag of the stream (relative to stream offset in LISR / HISR) */
#define DMA_FLAG_DME                ( DMA_LISR_DMEIF0 )

/** Transfer error flag of the stream (relative to stream offset in LISR / HISR) */
#define DMA_FLAG_TE                 ( DMA_LISR_TEIF0 )

/** Half transfer flag of the stream (relative to stream offset in LISR / HISR) */
#define DMA_FLAG_HT                 ( DMA_LISR_HTIF0 )

/** Transfer complete flag of the stream (relative to stream offset in LISR / HISR) */
#define DMA_FLAG_TC                 ( DMA_LISR_TCIF0 )

/** All flags of the stream */
#define DMA_FLAG_ALL                ( DMA_FLAG_FE | DMA_FLAG_DME | DMA_FLAG_TE | DMA_FLAG_HT | DMA_FLAG_TC )

/** Device revision ID (DBGMCU IDCODE REV_ID field) of revision V - first revision with
 *  bufferable transfers (TRBUFF) of STM32H742 / H743 / H745 / H747 / H750 / H753 / H755 / H757 */
#define DMA_DEV_REV_ID_V            ( 0x20000000u )

/** Interrupt enable bits of the stream in CR register */
#define DMA_STREAM_IT_ALL           ( DMA_SxCR_TCIE | DMA_SxCR_HTIE | DMA_SxCR_TEIE | DMA_SxCR_DMEIE )

/* =============================== MACROS =================================== */

/** Stream registers of selected DMA peripheral and stream */
#define DMA_STREAM_REG( dmaBus, dmaChannel )    ( (DMA_Stream_TypeDef *)( (uintptr_t)dma_PeriphConf[ dmaBus ].DmaReg + \
                                                  dma_StreamRegOffsetLut[ dmaChannel ] ) )

/* ============================== TYPEDEFS ================================== */

/** \brief Interrupt Status Routine (ISR) prototype */
typedef void (*dma_IsrHandler_t)( void );

/** \brief Structure type used for storing Interrupt Status Routine (ISR) callback's */
typedef struct
{
    dma_IsrCallback TransferCompleteCallback; /**< Transfer complete callback */
    dma_IsrCallback HalfTransferCallback;     /**< Half transfer callback     */
    dma_IsrCallback TransferErrorCallback;    /**< Transfer error callback    */
}   dma_IsrCallbacks_t;

/** \brief Structure type used for storing streams configuration */
typedef struct
{
    dma_ChannelId_t      StreamId;         /**< Stream identification                                */
    nvic_PeriphIrqList_t StreamIrq;        /**< Stream interrupt identification used by NVIC module  */
    uint32_t             StreamReg;        /**< Stream identification used for register operations   */
    dma_IsrHandler_t     StreamIsrHandler; /**< Stream Interrupt Status Routine (ISR) handler        */
}   dma_StreamsConfig_t;

/** \brief Structure type used by DMA configuration array */
typedef struct
{
          dma_PeriphId_t        DmaPeriphId;          /**< DMA bus identification    */
          rcc_PeriphId_t        RccPeriphId;          /**< RCC bus identification    */
          DMA_TypeDef          *DmaReg;               /**< DMA bus register address  */
    const dma_StreamsConfig_t * const StreamsConfig;  /**< DMA streams configuration */
    volatile dma_IsrCallbacks_t* const IsrCallbacks;  /**< ISR callback's array      */
}   dma_PeriphConfig_t;

/* ======================== FORWARD DECLARATIONS ============================ */

static uint32_t Dma_Get_Flags                ( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel );
static void     Dma_Clear_Flags              ( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, uint32_t flags );
static dma_RequestState_t Dma_Set_FifoMode   ( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_FunctionState_t fifoState );
static dma_FunctionState_t Dma_Get_UartRequest( dma_PeriphReqId_t requestId );
static void     Dma_Set_BufferableTransfer   ( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_PeriphReqId_t requestId );

static void Dma_GlobalIsrHandler( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel );

static void Dma_Dma1Stream0_IsrHandler( void );
static void Dma_Dma1Stream1_IsrHandler( void );
static void Dma_Dma1Stream2_IsrHandler( void );
static void Dma_Dma1Stream3_IsrHandler( void );
static void Dma_Dma1Stream4_IsrHandler( void );
static void Dma_Dma1Stream5_IsrHandler( void );
static void Dma_Dma1Stream6_IsrHandler( void );
static void Dma_Dma1Stream7_IsrHandler( void );
#if defined(DMA2)
static void Dma_Dma2Stream0_IsrHandler( void );
static void Dma_Dma2Stream1_IsrHandler( void );
static void Dma_Dma2Stream2_IsrHandler( void );
static void Dma_Dma2Stream3_IsrHandler( void );
static void Dma_Dma2Stream4_IsrHandler( void );
static void Dma_Dma2Stream5_IsrHandler( void );
static void Dma_Dma2Stream6_IsrHandler( void );
static void Dma_Dma2Stream7_IsrHandler( void );
#endif

/* ========================== EXPORTED VARIABLES ============================ */

/* =========================== LOCAL VARIABLES ============================== */

/** UART / USART requests served with bufferable transfers (device errata "UART / USART - DMA transfer lock") */
static const dma_PeriphReqId_t              dma_UartRequestLut[] =
{
    DMA_REQ_USART1_RX, DMA_REQ_USART1_TX,
    DMA_REQ_USART2_RX, DMA_REQ_USART2_TX,
    DMA_REQ_USART3_RX, DMA_REQ_USART3_TX,
    DMA_REQ_UART4_RX,  DMA_REQ_UART4_TX,
    DMA_REQ_UART5_RX,  DMA_REQ_UART5_TX,
    DMA_REQ_USART6_RX, DMA_REQ_USART6_TX,
    DMA_REQ_UART7_RX,  DMA_REQ_UART7_TX,
    DMA_REQ_UART8_RX,  DMA_REQ_UART8_TX,
#if defined(LL_DMAMUX1_REQ_UART9_RX)
    DMA_REQ_UART9_RX,  DMA_REQ_UART9_TX,
#endif
#if defined(LL_DMAMUX1_REQ_USART10_RX)
    DMA_REQ_USART10_RX, DMA_REQ_USART10_TX,
#endif
};

/** Offset of stream flags in LISR / HISR (and LIFCR / HIFCR) registers */
static const uint8_t                        dma_FlagOffsetLut[ DMA_ISR_STREAM_CNT ] =
{
    DMA_LISR_FEIF0_Pos,
    DMA_LISR_FEIF1_Pos,
    DMA_LISR_FEIF2_Pos,
    DMA_LISR_FEIF3_Pos
};

/** Offset of stream registers from DMA peripheral base address */
static const uint8_t                        dma_StreamRegOffsetLut[ DMA_STREAM_CNT ] =
{
    (uint8_t)( DMA1_Stream0_BASE - DMA1_BASE ),
    (uint8_t)( DMA1_Stream1_BASE - DMA1_BASE ),
    (uint8_t)( DMA1_Stream2_BASE - DMA1_BASE ),
    (uint8_t)( DMA1_Stream3_BASE - DMA1_BASE ),
    (uint8_t)( DMA1_Stream4_BASE - DMA1_BASE ),
    (uint8_t)( DMA1_Stream5_BASE - DMA1_BASE ),
    (uint8_t)( DMA1_Stream6_BASE - DMA1_BASE ),
    (uint8_t)( DMA1_Stream7_BASE - DMA1_BASE )
};

static volatile dma_IsrCallbacks_t          dma_Dma1IrqCallbacks[ DMA_STREAM_CNT ] =
{
    { .TransferCompleteCallback = DMA_NULL_PTR , .HalfTransferCallback = DMA_NULL_PTR , .TransferErrorCallback = DMA_NULL_PTR },
    { .TransferCompleteCallback = DMA_NULL_PTR , .HalfTransferCallback = DMA_NULL_PTR , .TransferErrorCallback = DMA_NULL_PTR },
    { .TransferCompleteCallback = DMA_NULL_PTR , .HalfTransferCallback = DMA_NULL_PTR , .TransferErrorCallback = DMA_NULL_PTR },
    { .TransferCompleteCallback = DMA_NULL_PTR , .HalfTransferCallback = DMA_NULL_PTR , .TransferErrorCallback = DMA_NULL_PTR },
    { .TransferCompleteCallback = DMA_NULL_PTR , .HalfTransferCallback = DMA_NULL_PTR , .TransferErrorCallback = DMA_NULL_PTR },
    { .TransferCompleteCallback = DMA_NULL_PTR , .HalfTransferCallback = DMA_NULL_PTR , .TransferErrorCallback = DMA_NULL_PTR },
    { .TransferCompleteCallback = DMA_NULL_PTR , .HalfTransferCallback = DMA_NULL_PTR , .TransferErrorCallback = DMA_NULL_PTR },
    { .TransferCompleteCallback = DMA_NULL_PTR , .HalfTransferCallback = DMA_NULL_PTR , .TransferErrorCallback = DMA_NULL_PTR }
};

#if defined(DMA2)
static volatile dma_IsrCallbacks_t          dma_Dma2IrqCallbacks[ DMA_STREAM_CNT ] =
{
    { .TransferCompleteCallback = DMA_NULL_PTR , .HalfTransferCallback = DMA_NULL_PTR , .TransferErrorCallback = DMA_NULL_PTR },
    { .TransferCompleteCallback = DMA_NULL_PTR , .HalfTransferCallback = DMA_NULL_PTR , .TransferErrorCallback = DMA_NULL_PTR },
    { .TransferCompleteCallback = DMA_NULL_PTR , .HalfTransferCallback = DMA_NULL_PTR , .TransferErrorCallback = DMA_NULL_PTR },
    { .TransferCompleteCallback = DMA_NULL_PTR , .HalfTransferCallback = DMA_NULL_PTR , .TransferErrorCallback = DMA_NULL_PTR },
    { .TransferCompleteCallback = DMA_NULL_PTR , .HalfTransferCallback = DMA_NULL_PTR , .TransferErrorCallback = DMA_NULL_PTR },
    { .TransferCompleteCallback = DMA_NULL_PTR , .HalfTransferCallback = DMA_NULL_PTR , .TransferErrorCallback = DMA_NULL_PTR },
    { .TransferCompleteCallback = DMA_NULL_PTR , .HalfTransferCallback = DMA_NULL_PTR , .TransferErrorCallback = DMA_NULL_PTR },
    { .TransferCompleteCallback = DMA_NULL_PTR , .HalfTransferCallback = DMA_NULL_PTR , .TransferErrorCallback = DMA_NULL_PTR }
};
#endif

static const dma_StreamsConfig_t            dma_Dma1IrqConfig[ DMA_STREAM_CNT ] =
{
    { .StreamId = DMA_STREAM_0 , .StreamIrq = NVIC_PERIPH_IRQ_DMA1_STREAM0 , .StreamReg = LL_DMA_STREAM_0 , .StreamIsrHandler = Dma_Dma1Stream0_IsrHandler },
    { .StreamId = DMA_STREAM_1 , .StreamIrq = NVIC_PERIPH_IRQ_DMA1_STREAM1 , .StreamReg = LL_DMA_STREAM_1 , .StreamIsrHandler = Dma_Dma1Stream1_IsrHandler },
    { .StreamId = DMA_STREAM_2 , .StreamIrq = NVIC_PERIPH_IRQ_DMA1_STREAM2 , .StreamReg = LL_DMA_STREAM_2 , .StreamIsrHandler = Dma_Dma1Stream2_IsrHandler },
    { .StreamId = DMA_STREAM_3 , .StreamIrq = NVIC_PERIPH_IRQ_DMA1_STREAM3 , .StreamReg = LL_DMA_STREAM_3 , .StreamIsrHandler = Dma_Dma1Stream3_IsrHandler },
    { .StreamId = DMA_STREAM_4 , .StreamIrq = NVIC_PERIPH_IRQ_DMA1_STREAM4 , .StreamReg = LL_DMA_STREAM_4 , .StreamIsrHandler = Dma_Dma1Stream4_IsrHandler },
    { .StreamId = DMA_STREAM_5 , .StreamIrq = NVIC_PERIPH_IRQ_DMA1_STREAM5 , .StreamReg = LL_DMA_STREAM_5 , .StreamIsrHandler = Dma_Dma1Stream5_IsrHandler },
    { .StreamId = DMA_STREAM_6 , .StreamIrq = NVIC_PERIPH_IRQ_DMA1_STREAM6 , .StreamReg = LL_DMA_STREAM_6 , .StreamIsrHandler = Dma_Dma1Stream6_IsrHandler },
    { .StreamId = DMA_STREAM_7 , .StreamIrq = NVIC_PERIPH_IRQ_DMA1_STREAM7 , .StreamReg = LL_DMA_STREAM_7 , .StreamIsrHandler = Dma_Dma1Stream7_IsrHandler },
};

#if defined(DMA2)
static const dma_StreamsConfig_t            dma_Dma2IrqConfig[ DMA_STREAM_CNT ] =
{
    { .StreamId = DMA_STREAM_0 , .StreamIrq = NVIC_PERIPH_IRQ_DMA2_STREAM0 , .StreamReg = LL_DMA_STREAM_0 , .StreamIsrHandler = Dma_Dma2Stream0_IsrHandler },
    { .StreamId = DMA_STREAM_1 , .StreamIrq = NVIC_PERIPH_IRQ_DMA2_STREAM1 , .StreamReg = LL_DMA_STREAM_1 , .StreamIsrHandler = Dma_Dma2Stream1_IsrHandler },
    { .StreamId = DMA_STREAM_2 , .StreamIrq = NVIC_PERIPH_IRQ_DMA2_STREAM2 , .StreamReg = LL_DMA_STREAM_2 , .StreamIsrHandler = Dma_Dma2Stream2_IsrHandler },
    { .StreamId = DMA_STREAM_3 , .StreamIrq = NVIC_PERIPH_IRQ_DMA2_STREAM3 , .StreamReg = LL_DMA_STREAM_3 , .StreamIsrHandler = Dma_Dma2Stream3_IsrHandler },
    { .StreamId = DMA_STREAM_4 , .StreamIrq = NVIC_PERIPH_IRQ_DMA2_STREAM4 , .StreamReg = LL_DMA_STREAM_4 , .StreamIsrHandler = Dma_Dma2Stream4_IsrHandler },
    { .StreamId = DMA_STREAM_5 , .StreamIrq = NVIC_PERIPH_IRQ_DMA2_STREAM5 , .StreamReg = LL_DMA_STREAM_5 , .StreamIsrHandler = Dma_Dma2Stream5_IsrHandler },
    { .StreamId = DMA_STREAM_6 , .StreamIrq = NVIC_PERIPH_IRQ_DMA2_STREAM6 , .StreamReg = LL_DMA_STREAM_6 , .StreamIsrHandler = Dma_Dma2Stream6_IsrHandler },
    { .StreamId = DMA_STREAM_7 , .StreamIrq = NVIC_PERIPH_IRQ_DMA2_STREAM7 , .StreamReg = LL_DMA_STREAM_7 , .StreamIsrHandler = Dma_Dma2Stream7_IsrHandler },
};
#endif

static const volatile dma_PeriphConfig_t    dma_PeriphConf[ DMA_PERIPH_CNT ] =
{
    { .DmaPeriphId = DMA_PERIPH_1 , .DmaReg = DMA1 , .RccPeriphId = RCC_PERIPH_DMA1 , .StreamsConfig = dma_Dma1IrqConfig , .IsrCallbacks = dma_Dma1IrqCallbacks },
#if defined(DMA2)
    { .DmaPeriphId = DMA_PERIPH_2 , .DmaReg = DMA2 , .RccPeriphId = RCC_PERIPH_DMA2 , .StreamsConfig = dma_Dma2IrqConfig , .IsrCallbacks = dma_Dma2IrqCallbacks },
#endif
};

/* ========================= EXPORTED FUNCTIONS ============================= */

/**
 * \brief Returns module SW version
 *
 * \return Module SW version
 */
dma_ModuleVersion_t Dma_Get_ModuleVersion( void )
{
    dma_ModuleVersion_t retVersion;

    retVersion.Major = DMA_MAJOR_VERSION;
    retVersion.Minor = DMA_MINOR_VERSION;
    retVersion.Patch = DMA_PATCH_VERSION;

    return (retVersion);
}


/**
 * \brief Direct Memory Access (DMA) configuration structure default value initialization
 *
 * \param dmaConfig [out]: Pointer to DMA configuration structure.
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Get_DefaultConfig( dma_ConfigStruct_t * const dmaConfig )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;

    if( DMA_NULL_PTR != dmaConfig )
    {
        dmaConfig->DmaPeriphId         = DMA_PERIPH_1;
        dmaConfig->DmaChannel          = DMA_STREAM_0;
        dmaConfig->Direction           = DMA_DIR_PERIPH_TO_MEMORY;
        dmaConfig->TransferMode        = DMA_TRANSFER_MODE_NORMAL;
        dmaConfig->PeriphAddress       = 0x00;
        dmaConfig->MemoryAddress       = 0x00;
        dmaConfig->PeriphAddrIncrement = DMA_PERIPH_ADDR_STATIC;
        dmaConfig->MemoryAddrIncrement = DMA_MEMORY_ADDR_STATIC;
        dmaConfig->PeriphTransferSize  = DMA_TRANSFER_SIZE_8BIT;
        dmaConfig->MemoryTransferSize  = DMA_TRANSFER_SIZE_8BIT;
        dmaConfig->DataCount           = 0u;
        dmaConfig->PeripheralReqId     = DMA_REQ_MEM2MEM;
        dmaConfig->Priority            = DMA_PRIORITY_LOW;

        dmaConfig->TransferCompleteCallback = DMA_NULL_PTR;
        dmaConfig->HalfTransferCallback     = DMA_NULL_PTR;
        dmaConfig->TransferErrorCallback    = DMA_NULL_PTR;

        retState = DMA_REQUEST_OK;
    }
    else
    {
        retState = DMA_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Direct Memory Access (DMA) initialization function through
 *        configuration structure.
 *
 * Activates DMA peripheral clock, disables the stream (configuration registers
 * are write protected while the stream is enabled) and configures it. FIFO is
 * enabled if peripheral and memory transfer sizes differ (data packing),
 * otherwise direct mode is used. Interrupt callbacks of the stream are replaced
 * by callbacks of the configuration (null pointer clears the callback).
 *
 * \param dmaConfig [in]: Configuration structure
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Init( dma_ConfigStruct_t * const dmaConfig )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;

    if( ( DMA_NULL_PTR    != dmaConfig              ) &&
        ( DMA_PERIPH_CNT   > dmaConfig->DmaPeriphId ) &&
        ( DMA_STREAM_CNT   > dmaConfig->DmaChannel  )    )
    {
        rcc_FunctionState_t rccActivationState = RCC_FUNCTION_INACTIVE;
        rcc_RequestState_t  rccRequestState    = RCC_REQUEST_ERROR;
        dma_FunctionState_t fifoState          = DMA_FUNCTION_INACTIVE;

        /*-------------- DMA peripheral clock activation section -------------*/
        rccRequestState = Rcc_Get_PeriphState( dma_PeriphConf[ dmaConfig->DmaPeriphId ].RccPeriphId, &rccActivationState );

        if( RCC_REQUEST_ERROR == rccRequestState )
        {
            retState = DMA_REQUEST_ERROR;
            return ( retState );
        }
        else if( RCC_FUNCTION_INACTIVE == rccActivationState )
        {
            rccRequestState = Rcc_Set_PeriphActive( dma_PeriphConf[ dmaConfig->DmaPeriphId ].RccPeriphId );

            if( RCC_REQUEST_ERROR == rccRequestState )
            {
                retState = DMA_REQUEST_ERROR;
                return ( retState );
            }
            else
            {
                /* No action required */
            }
        }
        else
        {
            /* No clock activation needed */
        }

        /*-------------------- DMA configuration section ---------------------*/
        retState = Dma_Set_TransferInactive( dmaConfig->DmaPeriphId, dmaConfig->DmaChannel );
        if( DMA_REQUEST_OK != retState )
        {
            return ( retState );
        }
        else
        {
            /* No action required */
        }

        retState = Dma_Set_PeripheralRequest( dmaConfig->DmaPeriphId, dmaConfig->DmaChannel, dmaConfig->PeripheralReqId );
        if( DMA_REQUEST_OK != retState )
        {
            return ( retState );
        }
        else
        {
            /* No action required */
        }

        retState = Dma_Set_TransferMode( dmaConfig->DmaPeriphId, dmaConfig->DmaChannel, dmaConfig->TransferMode );
        if( DMA_REQUEST_OK != retState )
        {
            return ( retState );
        }
        else
        {
            /* No action required */
        }

        retState = Dma_Set_Direction( dmaConfig->DmaPeriphId, dmaConfig->DmaChannel, dmaConfig->Direction );
        if( DMA_REQUEST_OK != retState )
        {
            return ( retState );
        }
        else
        {
            /* No action required */
        }

        retState = Dma_Set_Priority( dmaConfig->DmaPeriphId, dmaConfig->DmaChannel, dmaConfig->Priority );
        if( DMA_REQUEST_OK != retState )
        {
            return ( retState );
        }
        else
        {
            /* No action required */
        }

        retState = Dma_Set_PeriphAddrIncrement( dmaConfig->DmaPeriphId, dmaConfig->DmaChannel, dmaConfig->PeriphAddrIncrement );
        if( DMA_REQUEST_OK != retState )
        {
            return ( retState );
        }
        else
        {
            /* No action required */
        }

        retState = Dma_Set_MemoryAddrIncrement( dmaConfig->DmaPeriphId, dmaConfig->DmaChannel, dmaConfig->MemoryAddrIncrement );
        if( DMA_REQUEST_OK != retState )
        {
            return ( retState );
        }
        else
        {
            /* No action required */
        }

        retState = Dma_Set_PeriphTransferSize( dmaConfig->DmaPeriphId, dmaConfig->DmaChannel, dmaConfig->PeriphTransferSize );
        if( DMA_REQUEST_OK != retState )
        {
            return ( retState );
        }
        else
        {
            /* No action required */
        }

        retState = Dma_Set_MemoryTransferSize( dmaConfig->DmaPeriphId, dmaConfig->DmaChannel, dmaConfig->MemoryTransferSize );
        if( DMA_REQUEST_OK != retState )
        {
            return ( retState );
        }
        else
        {
            /* No action required */
        }

        if( dmaConfig->PeriphTransferSize != dmaConfig->MemoryTransferSize )
        {
            fifoState = DMA_FUNCTION_ACTIVE;
        }
        else
        {
            fifoState = DMA_FUNCTION_INACTIVE;
        }

        retState = Dma_Set_FifoMode( dmaConfig->DmaPeriphId, dmaConfig->DmaChannel, fifoState );
        if( DMA_REQUEST_OK != retState )
        {
            return ( retState );
        }
        else
        {
            /* No action required */
        }

        retState = Dma_Set_PeriphAddr( dmaConfig->DmaPeriphId, dmaConfig->DmaChannel, dmaConfig->PeriphAddress );
        if( DMA_REQUEST_OK != retState )
        {
            return ( retState );
        }
        else
        {
            /* No action required */
        }

        retState = Dma_Set_MemoryAddr( dmaConfig->DmaPeriphId, dmaConfig->DmaChannel, dmaConfig->MemoryAddress );
        if( DMA_REQUEST_OK != retState )
        {
            return ( retState );
        }
        else
        {
            /* No action required */
        }

        retState = Dma_Set_DataCount( dmaConfig->DmaPeriphId, dmaConfig->DmaChannel, dmaConfig->DataCount );
        if( DMA_REQUEST_OK != retState )
        {
            return ( retState );
        }
        else
        {
            /* No action required */
        }

        retState = Dma_Set_TransferCompleteIsrHandler( dmaConfig->DmaPeriphId, dmaConfig->DmaChannel, dmaConfig->TransferCompleteCallback );
        if( DMA_REQUEST_OK != retState )
        {
            return ( retState );
        }
        else
        {
            /* No action required */
        }

        retState = Dma_Set_HalfTransferIsrHandler( dmaConfig->DmaPeriphId, dmaConfig->DmaChannel, dmaConfig->HalfTransferCallback );
        if( DMA_REQUEST_OK != retState )
        {
            return ( retState );
        }
        else
        {
            /* No action required */
        }

        retState = Dma_Set_TransferErrorIsrHandler( dmaConfig->DmaPeriphId, dmaConfig->DmaChannel, dmaConfig->TransferErrorCallback );
    }
    else
    {
        retState = DMA_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Activates transfer of selected DMA stream
 *
 * All flags of the stream are cleared before the stream is enabled (required
 * by hardware). Request succeeds if the stream is enabled or the transfer has
 * already completed (short memory to memory transfer). Transfer error detected
 * after the stream enable is reported as error.
 *
 * \param dmaBus         [in]: DMA peripheral ID
 * \param dmaChannel     [in]: DMA stream ID
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Set_TransferActive( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel )
{
    dma_RequestState_t retState  = DMA_REQUEST_ERROR;
    uint32_t           regValue  = 0u;
    uint32_t           flagValue = 0u;

    if( ( DMA_PERIPH_CNT > dmaBus     ) &&
        ( DMA_STREAM_CNT > dmaChannel )    )
    {
        Dma_Clear_Flags( dmaBus, dmaChannel, DMA_FLAG_ALL );

        LL_DMA_EnableStream( dma_PeriphConf[ dmaBus ].DmaReg,
                             dma_PeriphConf[ dmaBus ].StreamsConfig[ dmaChannel ].StreamReg );

        for( uint32_t iterationCnt = 0u; DMA_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
        {
            regValue  = LL_DMA_IsEnabledStream( dma_PeriphConf[ dmaBus ].DmaReg,
                                                dma_PeriphConf[ dmaBus ].StreamsConfig[ dmaChannel ].StreamReg );
            flagValue = Dma_Get_Flags( dmaBus, dmaChannel );

            const uint32_t dataLength = LL_DMA_GetDataLength( dma_PeriphConf[ dmaBus ].DmaReg,
                                                              dma_PeriphConf[ dmaBus ].StreamsConfig[ dmaChannel ].StreamReg );

            if( 0u != ( DMA_FLAG_TE & flagValue ) )
            {
                /* Stream was disabled by hardware due to transfer error */
                retState = DMA_REQUEST_ERROR;
                break;
            }
            else if( ( 0u != regValue                    ) ||
                     ( 0u != ( DMA_FLAG_TC & flagValue ) ) ||
                     ( 0u == dataLength                  )    )
            {
                /* Stream is running or the transfer has already completed */
                retState = DMA_REQUEST_OK;
                break;
            }
            else
            {
                /* Stream has not yet been enabled, keep return state as error */
                retState = DMA_REQUEST_ERROR;
            }
        }
    }
    else
    {
        retState = DMA_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief De-activates transfer of selected DMA stream
 *
 * Interrupts of the stream are masked while the stream is disabled, flags set
 * by the abort of the transfer (transfer complete is set by hardware when the
 * stream is disabled by software) are cleared. Callbacks are not executed for
 * aborted transfer.
 *
 * \param dmaBus         [in]: DMA peripheral ID
 * \param dmaChannel     [in]: DMA stream ID
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Set_TransferInactive( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel )
{
    dma_RequestState_t  retState  = DMA_REQUEST_ERROR;
    uint32_t            regValue  = 0u;
    uint32_t            itEnabled = 0u;
    DMA_Stream_TypeDef *streamReg = DMA_NULL_PTR;

    if( ( DMA_PERIPH_CNT > dmaBus     ) &&
        ( DMA_STREAM_CNT > dmaChannel )    )
    {
        streamReg = DMA_STREAM_REG( dmaBus, dmaChannel );
        itEnabled = READ_BIT( streamReg->CR, DMA_STREAM_IT_ALL );

        CLEAR_BIT( streamReg->CR, DMA_STREAM_IT_ALL );

        LL_DMA_DisableStream( dma_PeriphConf[ dmaBus ].DmaReg,
                              dma_PeriphConf[ dmaBus ].StreamsConfig[ dmaChannel ].StreamReg );

        for( uint32_t iterationCnt = 0u; DMA_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
        {
            regValue = LL_DMA_IsEnabledStream( dma_PeriphConf[ dmaBus ].DmaReg,
                                               dma_PeriphConf[ dmaBus ].StreamsConfig[ dmaChannel ].StreamReg );

            if( 0u == regValue )
            {
                retState = DMA_REQUEST_OK;
                break;
            }
            else
            {
                /* Current data transfer has not yet been finished, keep return state as error */
                retState = DMA_REQUEST_ERROR;
            }
        }

        if( DMA_REQUEST_OK == retState )
        {
            Dma_Clear_Flags( dmaBus, dmaChannel, DMA_FLAG_ALL );
        }
        else
        {
            /* Stream is still enabled, flags of the transfer are kept */
        }

        SET_BIT( streamReg->CR, itEnabled );
    }
    else
    {
        retState = DMA_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Returns transfer state of selected DMA stream
 *
 * \param dmaBus         [in]: DMA peripheral ID
 * \param dmaChannel     [in]: DMA stream ID
 * \param transferState [out]: Stream active/inactive state
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Get_TransferState( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_FunctionState_t * const transferState )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;
    uint32_t           regValue = 0u;

    if( ( DMA_PERIPH_CNT > dmaBus        ) &&
        ( DMA_STREAM_CNT > dmaChannel    ) &&
        ( DMA_NULL_PTR  != transferState )    )
    {
        regValue = LL_DMA_IsEnabledStream( dma_PeriphConf[ dmaBus ].DmaReg,
                                           dma_PeriphConf[ dmaBus ].StreamsConfig[ dmaChannel ].StreamReg );

        if( 0u != regValue )
        {
            *transferState = DMA_FUNCTION_ACTIVE;
        }
        else
        {
            *transferState = DMA_FUNCTION_INACTIVE;
        }

        retState = DMA_REQUEST_OK;
    }
    else
    {
        retState = DMA_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Enables required stream for selected DMA peripheral
 *
 * Equivalent of \ref Dma_Set_TransferActive (stream enable starts transfer).
 *
 * \param dmaBus     [in]: DMA peripheral ID
 * \param dmaChannel [in]: DMA stream ID
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Set_ChannelActive( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel )
{
    return ( Dma_Set_TransferActive( dmaBus, dmaChannel ) );
}


/**
 * \brief Disables required stream for selected DMA peripheral
 *
 * Equivalent of \ref Dma_Set_TransferInactive.
 *
 * \param dmaBus     [in]: DMA peripheral ID
 * \param dmaChannel [in]: DMA stream ID
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Set_ChannelInactive( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel )
{
    return ( Dma_Set_TransferInactive( dmaBus, dmaChannel ) );
}


/**
 * \brief Reads activation status of required stream for selected DMA peripheral
 *
 * DMA stream can be activated/deactivated as user needs. This function provide
 * ability to read this activation status.
 *
 * \param dmaBus        [in]: DMA peripheral ID
 * \param dmaChannel    [in]: DMA stream ID
 * \param channelState [out]: Activation status
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Get_ChannelState( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_FunctionState_t * const channelState )
{
    return ( Dma_Get_TransferState( dmaBus, dmaChannel, channelState ) );
}


/**
 * \brief Configures transfer direction of DMA stream.
 *
 * DMA stream can be configured in transferring multiple directions. It can
 * move data from peripheral to memory, from memory to peripheral and from
 * memory to memory (DMA1 and DMA2).
 *
 * \param dmaBus     [in]: DMA peripheral ID
 * \param dmaChannel [in]: DMA stream ID
 * \param direction  [in]: Transfer direction configuration
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Set_Direction( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_Direction_t direction )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;
    uint32_t           regValue = 0u;

    if( ( DMA_PERIPH_CNT > dmaBus     ) &&
        ( DMA_STREAM_CNT > dmaChannel ) &&
        ( ( DMA_DIR_PERIPH_TO_MEMORY == direction ) ||
          ( DMA_DIR_MEMORY_TO_PERIPH == direction ) ||
          ( DMA_DIR_MEMORY_TO_MEMORY == direction ) ) )
    {
        const uint32_t streamEnabled = LL_DMA_IsEnabledStream( dma_PeriphConf[ dmaBus ].DmaReg,
                                                               dma_PeriphConf[ dmaBus ].StreamsConfig[ dmaChannel ].StreamReg );

        if( 0u != streamEnabled )
        {
            /* Configuration is write protected while the stream is enabled */
            retState = DMA_REQUEST_ERROR;
        }
        else
        {
            LL_DMA_SetDataTransferDirection( dma_PeriphConf[ dmaBus ].DmaReg,
                                             dma_PeriphConf[ dmaBus ].StreamsConfig[ dmaChannel ].StreamReg,
                                             direction );

            for( uint32_t iterationCnt = 0u; DMA_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
            {
                regValue = LL_DMA_GetDataTransferDirection( dma_PeriphConf[ dmaBus ].DmaReg,
                                                            dma_PeriphConf[ dmaBus ].StreamsConfig[ dmaChannel ].StreamReg );

                if( direction == regValue )
                {
                    retState = DMA_REQUEST_OK;
                    break;
                }
                else
                {
                    /* Register has not yet been changed, keep return state as error */
                    retState = DMA_REQUEST_ERROR;
                }
            }
        }
    }
    else
    {
        retState = DMA_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Returns transfer direction of DMA stream.
 *
 * \param dmaBus      [in]: DMA peripheral ID
 * \param dmaChannel  [in]: DMA stream ID
 * \param direction  [out]: Transfer direction configuration
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Get_Direction( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_Direction_t * const direction )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;

    if( ( DMA_PERIPH_CNT > dmaBus     ) &&
        ( DMA_STREAM_CNT > dmaChannel ) &&
        ( DMA_NULL_PTR  != direction  )    )
    {
        *direction = LL_DMA_GetDataTransferDirection( dma_PeriphConf[ dmaBus ].DmaReg,
                                                      dma_PeriphConf[ dmaBus ].StreamsConfig[ dmaChannel ].StreamReg );

        retState = DMA_REQUEST_OK;
    }
    else
    {
        retState = DMA_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Configures transfer mode of DMA stream.
 *
 * DMA stream can be configured in transferring multiple modes. It can
 * be stopped, after reaching target count of transfers, or it can start from
 * beginning of target location and fill target location again.
 *
 * \param dmaBus       [in]: DMA peripheral ID
 * \param dmaChannel   [in]: DMA stream ID
 * \param transferMode [in]: Transfer mode (normal or circular) configuration
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Set_TransferMode( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_TransferMode_t transferMode )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;
    uint32_t           regValue = 0u;

    if( ( DMA_PERIPH_CNT > dmaBus     ) &&
        ( DMA_STREAM_CNT > dmaChannel ) &&
        ( ( DMA_TRANSFER_MODE_NORMAL   == transferMode ) ||
          ( DMA_TRANSFER_MODE_CIRCULAR == transferMode )    ) )
    {
        const uint32_t streamEnabled = LL_DMA_IsEnabledStream( dma_PeriphConf[ dmaBus ].DmaReg,
                                                               dma_PeriphConf[ dmaBus ].StreamsConfig[ dmaChannel ].StreamReg );

        if( 0u != streamEnabled )
        {
            /* Configuration is write protected while the stream is enabled */
            retState = DMA_REQUEST_ERROR;
        }
        else
        {
            LL_DMA_SetMode( dma_PeriphConf[ dmaBus ].DmaReg,
                            dma_PeriphConf[ dmaBus ].StreamsConfig[ dmaChannel ].StreamReg,
                            transferMode );

            for( uint32_t iterationCnt = 0u; DMA_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
            {
                regValue = LL_DMA_GetMode( dma_PeriphConf[ dmaBus ].DmaReg,
                                           dma_PeriphConf[ dmaBus ].StreamsConfig[ dmaChannel ].StreamReg );

                if( transferMode == regValue )
                {
                    retState = DMA_REQUEST_OK;
                    break;
                }
                else
                {
                    /* Register has not yet been changed, keep return state as error */
                    retState = DMA_REQUEST_ERROR;
                }
            }
        }
    }
    else
    {
        retState = DMA_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Returns transfer mode of DMA stream.
 *
 * DMA stream can be configured in transferring multiple modes. It can
 * be stopped, after reaching target count of transfers, or it can start from
 * beginning of target location and fill target location again.
 *
 * \param dmaBus        [in]: DMA peripheral ID
 * \param dmaChannel    [in]: DMA stream ID
 * \param transferMode [out]: Transfer mode (normal or circular) configuration
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Get_TransferMode( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_TransferMode_t * const transferMode )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;

    if( ( DMA_PERIPH_CNT > dmaBus       ) &&
        ( DMA_STREAM_CNT > dmaChannel   ) &&
        ( DMA_NULL_PTR  != transferMode )    )
    {
        *transferMode = LL_DMA_GetMode( dma_PeriphConf[ dmaBus ].DmaReg,
                                        dma_PeriphConf[ dmaBus ].StreamsConfig[ dmaChannel ].StreamReg );

        retState = DMA_REQUEST_OK;
    }
    else
    {
        retState = DMA_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Configures DMA peripheral address.
 *
 * \param dmaBus     [in]: DMA peripheral ID
 * \param dmaChannel [in]: DMA stream ID
 * \param periphAddr [in]: Address of peripheral used for data transfer
 *                         (source address in memory to memory direction)
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Set_PeriphAddr( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_PeriphAddr_t periphAddr )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;
    uint32_t           regValue = 0u;

    if( ( DMA_PERIPH_CNT > dmaBus     ) &&
        ( DMA_STREAM_CNT > dmaChannel )    )
    {
        const uint32_t streamEnabled = LL_DMA_IsEnabledStream( dma_PeriphConf[ dmaBus ].DmaReg,
                                                               dma_PeriphConf[ dmaBus ].StreamsConfig[ dmaChannel ].StreamReg );

        if( 0u != streamEnabled )
        {
            /* Configuration is write protected while the stream is enabled */
            retState = DMA_REQUEST_ERROR;
        }
        else
        {
            LL_DMA_SetPeriphAddress( dma_PeriphConf[ dmaBus ].DmaReg,
                                     dma_PeriphConf[ dmaBus ].StreamsConfig[ dmaChannel ].StreamReg,
                                     periphAddr );

            for( uint32_t iterationCnt = 0u; DMA_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
            {
                regValue = LL_DMA_GetPeriphAddress( dma_PeriphConf[ dmaBus ].DmaReg,
                                                    dma_PeriphConf[ dmaBus ].StreamsConfig[ dmaChannel ].StreamReg );

                if( periphAddr == regValue )
                {
                    retState = DMA_REQUEST_OK;
                    break;
                }
                else
                {
                    /* Register has not yet been changed, keep return state as error */
                    retState = DMA_REQUEST_ERROR;
                }
            }
        }
    }
    else
    {
        retState = DMA_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Returns DMA peripheral address.
 *
 * \param dmaBus      [in]: DMA peripheral ID
 * \param dmaChannel  [in]: DMA stream ID
 * \param periphAddr [out]: Address of peripheral used for data transfer
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Get_PeriphAddr( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_PeriphAddr_t * const periphAddr )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;

    if( ( DMA_PERIPH_CNT > dmaBus     ) &&
        ( DMA_STREAM_CNT > dmaChannel ) &&
        ( DMA_NULL_PTR  != periphAddr )    )
    {
        *periphAddr = LL_DMA_GetPeriphAddress( dma_PeriphConf[ dmaBus ].DmaReg,
                                               dma_PeriphConf[ dmaBus ].StreamsConfig[ dmaChannel ].StreamReg );

        retState = DMA_REQUEST_OK;
    }
    else
    {
        retState = DMA_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Configures DMA memory address.
 *
 * \param dmaBus     [in]: DMA peripheral ID
 * \param dmaChannel [in]: DMA stream ID
 * \param memoryAddr [in]: Memory address used for data transfer
 *                         (destination address in memory to memory direction)
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Set_MemoryAddr( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_MemoryAddr_t memoryAddr )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;
    uint32_t           regValue = 0u;

    if( ( DMA_PERIPH_CNT > dmaBus     ) &&
        ( DMA_STREAM_CNT > dmaChannel )    )
    {
        const uint32_t streamEnabled = LL_DMA_IsEnabledStream( dma_PeriphConf[ dmaBus ].DmaReg,
                                                               dma_PeriphConf[ dmaBus ].StreamsConfig[ dmaChannel ].StreamReg );

        if( 0u != streamEnabled )
        {
            /* Configuration is write protected while the stream is enabled */
            retState = DMA_REQUEST_ERROR;
        }
        else
        {
            LL_DMA_SetMemoryAddress( dma_PeriphConf[ dmaBus ].DmaReg,
                                     dma_PeriphConf[ dmaBus ].StreamsConfig[ dmaChannel ].StreamReg,
                                     memoryAddr );

            for( uint32_t iterationCnt = 0u; DMA_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
            {
                regValue = LL_DMA_GetMemoryAddress( dma_PeriphConf[ dmaBus ].DmaReg,
                                                    dma_PeriphConf[ dmaBus ].StreamsConfig[ dmaChannel ].StreamReg );

                if( memoryAddr == regValue )
                {
                    retState = DMA_REQUEST_OK;
                    break;
                }
                else
                {
                    /* Register has not yet been changed, keep return state as error */
                    retState = DMA_REQUEST_ERROR;
                }
            }
        }
    }
    else
    {
        retState = DMA_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Returns DMA memory address.
 *
 * \param dmaBus      [in]: DMA peripheral ID
 * \param dmaChannel  [in]: DMA stream ID
 * \param memoryAddr [out]: Memory address used for data transfer
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Get_MemoryAddr( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_MemoryAddr_t * const memoryAddr )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;

    if( ( DMA_PERIPH_CNT > dmaBus     ) &&
        ( DMA_STREAM_CNT > dmaChannel ) &&
        ( DMA_NULL_PTR  != memoryAddr )    )
    {
        *memoryAddr = LL_DMA_GetMemoryAddress( dma_PeriphConf[ dmaBus ].DmaReg,
                                               dma_PeriphConf[ dmaBus ].StreamsConfig[ dmaChannel ].StreamReg );

        retState = DMA_REQUEST_OK;
    }
    else
    {
        retState = DMA_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Configures DMA peripheral address increment.
 *
 * \param dmaBus        [in]: DMA peripheral ID
 * \param dmaChannel    [in]: DMA stream ID
 * \param periphAddrInc [in]: Peripheral address increment state
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Set_PeriphAddrIncrement( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_PeriphAddrInc_t periphAddrInc )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;
    uint32_t           regValue = 0u;

    if( ( DMA_PERIPH_CNT > dmaBus     ) &&
        ( DMA_STREAM_CNT > dmaChannel ) &&
        ( ( DMA_PERIPH_ADDR_INCREMENT == periphAddrInc ) ||
          ( DMA_PERIPH_ADDR_STATIC    == periphAddrInc )    ) )
    {
        const uint32_t streamEnabled = LL_DMA_IsEnabledStream( dma_PeriphConf[ dmaBus ].DmaReg,
                                                               dma_PeriphConf[ dmaBus ].StreamsConfig[ dmaChannel ].StreamReg );

        if( 0u != streamEnabled )
        {
            /* Configuration is write protected while the stream is enabled */
            retState = DMA_REQUEST_ERROR;
        }
        else
        {
            LL_DMA_SetPeriphIncMode( dma_PeriphConf[ dmaBus ].DmaReg,
                                     dma_PeriphConf[ dmaBus ].StreamsConfig[ dmaChannel ].StreamReg,
                                     periphAddrInc );

            for( uint32_t iterationCnt = 0u; DMA_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
            {
                regValue = LL_DMA_GetPeriphIncMode( dma_PeriphConf[ dmaBus ].DmaReg,
                                                    dma_PeriphConf[ dmaBus ].StreamsConfig[ dmaChannel ].StreamReg );

                if( periphAddrInc == regValue )
                {
                    retState = DMA_REQUEST_OK;
                    break;
                }
                else
                {
                    /* Register has not yet been changed, keep return state as error */
                    retState = DMA_REQUEST_ERROR;
                }
            }
        }
    }
    else
    {
        retState = DMA_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Returns DMA peripheral address increment.
 *
 * \param dmaBus         [in]: DMA peripheral ID
 * \param dmaChannel     [in]: DMA stream ID
 * \param periphAddrInc [out]: Peripheral address increment state
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Get_PeriphAddrIncrement( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_PeriphAddrInc_t * const periphAddrInc )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;

    if( ( DMA_PERIPH_CNT > dmaBus        ) &&
        ( DMA_STREAM_CNT > dmaChannel    ) &&
        ( DMA_NULL_PTR  != periphAddrInc )    )
    {
        *periphAddrInc = LL_DMA_GetPeriphIncMode( dma_PeriphConf[ dmaBus ].DmaReg,
                                                  dma_PeriphConf[ dmaBus ].StreamsConfig[ dmaChannel ].StreamReg );

        retState = DMA_REQUEST_OK;
    }
    else
    {
        retState = DMA_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Configures DMA memory address increment.
 *
 * \param dmaBus        [in]: DMA peripheral ID
 * \param dmaChannel    [in]: DMA stream ID
 * \param memoryAddrInc [in]: Memory address increment state
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Set_MemoryAddrIncrement( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_MemoryAddrInc_t memoryAddrInc )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;
    uint32_t           regValue = 0u;

    if( ( DMA_PERIPH_CNT > dmaBus     ) &&
        ( DMA_STREAM_CNT > dmaChannel ) &&
        ( ( DMA_MEMORY_ADDR_INCREMENT == memoryAddrInc ) ||
          ( DMA_MEMORY_ADDR_STATIC    == memoryAddrInc )    ) )
    {
        const uint32_t streamEnabled = LL_DMA_IsEnabledStream( dma_PeriphConf[ dmaBus ].DmaReg,
                                                               dma_PeriphConf[ dmaBus ].StreamsConfig[ dmaChannel ].StreamReg );

        if( 0u != streamEnabled )
        {
            /* Configuration is write protected while the stream is enabled */
            retState = DMA_REQUEST_ERROR;
        }
        else
        {
            LL_DMA_SetMemoryIncMode( dma_PeriphConf[ dmaBus ].DmaReg,
                                     dma_PeriphConf[ dmaBus ].StreamsConfig[ dmaChannel ].StreamReg,
                                     memoryAddrInc );

            for( uint32_t iterationCnt = 0u; DMA_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
            {
                regValue = LL_DMA_GetMemoryIncMode( dma_PeriphConf[ dmaBus ].DmaReg,
                                                    dma_PeriphConf[ dmaBus ].StreamsConfig[ dmaChannel ].StreamReg );

                if( memoryAddrInc == regValue )
                {
                    retState = DMA_REQUEST_OK;
                    break;
                }
                else
                {
                    /* Register has not yet been changed, keep return state as error */
                    retState = DMA_REQUEST_ERROR;
                }
            }
        }
    }
    else
    {
        retState = DMA_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Returns DMA memory address increment.
 *
 * \param dmaBus         [in]: DMA peripheral ID
 * \param dmaChannel     [in]: DMA stream ID
 * \param memoryAddrInc [out]: Memory address increment state
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Get_MemoryAddrIncrement( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_MemoryAddrInc_t * const memoryAddrInc )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;

    if( ( DMA_PERIPH_CNT > dmaBus        ) &&
        ( DMA_STREAM_CNT > dmaChannel    ) &&
        ( DMA_NULL_PTR  != memoryAddrInc )    )
    {
        *memoryAddrInc = LL_DMA_GetMemoryIncMode( dma_PeriphConf[ dmaBus ].DmaReg,
                                                  dma_PeriphConf[ dmaBus ].StreamsConfig[ dmaChannel ].StreamReg );

        retState = DMA_REQUEST_OK;
    }
    else
    {
        retState = DMA_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Configures DMA peripheral transfer size.
 *
 * \param dmaBus             [in]: DMA peripheral ID
 * \param dmaChannel         [in]: DMA stream ID
 * \param periphTransferSize [in]: Peripheral transfer size.
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Set_PeriphTransferSize( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_TransferSize_t periphTransferSize )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;
    uint32_t           regValue = 0u;
    uint32_t           reqValue = 0u;

    if( ( DMA_PERIPH_CNT        > dmaBus             ) &&
        ( DMA_STREAM_CNT        > dmaChannel         ) &&
        ( DMA_TRANSFER_SIZE_CNT > periphTransferSize )    )
    {
        const uint32_t streamEnabled = LL_DMA_IsEnabledStream( dma_PeriphConf[ dmaBus ].DmaReg,
                                                               dma_PeriphConf[ dmaBus ].StreamsConfig[ dmaChannel ].StreamReg );

        if( 0u != streamEnabled )
        {
            /* Configuration is write protected while the stream is enabled */
            retState = DMA_REQUEST_ERROR;
        }
        else
        {
            if( DMA_TRANSFER_SIZE_32BIT == periphTransferSize )
            {
                reqValue = LL_DMA_PDATAALIGN_WORD;
            }
            else if( DMA_TRANSFER_SIZE_16BIT == periphTransferSize )
            {
                reqValue = LL_DMA_PDATAALIGN_HALFWORD;
            }
            else
            {
                reqValue = LL_DMA_PDATAALIGN_BYTE;
            }

            LL_DMA_SetPeriphSize( dma_PeriphConf[ dmaBus ].DmaReg,
                                  dma_PeriphConf[ dmaBus ].StreamsConfig[ dmaChannel ].StreamReg,
                                  reqValue );

            for( uint32_t iterationCnt = 0u; DMA_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
            {
                regValue = LL_DMA_GetPeriphSize( dma_PeriphConf[ dmaBus ].DmaReg,
                                                 dma_PeriphConf[ dmaBus ].StreamsConfig[ dmaChannel ].StreamReg );

                if( reqValue == regValue )
                {
                    retState = DMA_REQUEST_OK;
                    break;
                }
                else
                {
                    /* Register has not yet been changed, keep return state as error */
                    retState = DMA_REQUEST_ERROR;
                }
            }
        }
    }
    else
    {
        retState = DMA_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Returns DMA peripheral transfer size.
 *
 * \param dmaBus              [in]: DMA peripheral ID
 * \param dmaChannel          [in]: DMA stream ID
 * \param periphTransferSize [out]: Peripheral transfer size
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Get_PeriphTransferSize( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_TransferSize_t * const periphTransferSize )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;
    uint32_t           regValue = 0u;

    if( ( DMA_PERIPH_CNT > dmaBus             ) &&
        ( DMA_STREAM_CNT > dmaChannel         ) &&
        ( DMA_NULL_PTR  != periphTransferSize )    )
    {
        regValue = LL_DMA_GetPeriphSize( dma_PeriphConf[ dmaBus ].DmaReg,
                                         dma_PeriphConf[ dmaBus ].StreamsConfig[ dmaChannel ].StreamReg );

        if( LL_DMA_PDATAALIGN_WORD == regValue )
        {
            *periphTransferSize = DMA_TRANSFER_SIZE_32BIT;
        }
        else if( LL_DMA_PDATAALIGN_HALFWORD == regValue )
        {
            *periphTransferSize = DMA_TRANSFER_SIZE_16BIT;
        }
        else
        {
            *periphTransferSize = DMA_TRANSFER_SIZE_8BIT;
        }

        retState = DMA_REQUEST_OK;
    }
    else
    {
        retState = DMA_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Configures DMA memory transfer size.
 *
 * \note In direct mode (FIFO disabled) hardware uses peripheral transfer size
 *       on memory side as well. \ref Dma_Init enables FIFO if sizes differ.
 *
 * \param dmaBus             [in]: DMA peripheral ID
 * \param dmaChannel         [in]: DMA stream ID
 * \param memoryTransferSize [in]: Memory transfer size.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Set_MemoryTransferSize( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_TransferSize_t memoryTransferSize )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;
    uint32_t           regValue = 0u;
    uint32_t           reqValue = 0u;

    if( ( DMA_PERIPH_CNT        > dmaBus             ) &&
        ( DMA_STREAM_CNT        > dmaChannel         ) &&
        ( DMA_TRANSFER_SIZE_CNT > memoryTransferSize )    )
    {
        const uint32_t streamEnabled = LL_DMA_IsEnabledStream( dma_PeriphConf[ dmaBus ].DmaReg,
                                                               dma_PeriphConf[ dmaBus ].StreamsConfig[ dmaChannel ].StreamReg );

        if( 0u != streamEnabled )
        {
            /* Configuration is write protected while the stream is enabled */
            retState = DMA_REQUEST_ERROR;
        }
        else
        {
            if( DMA_TRANSFER_SIZE_32BIT == memoryTransferSize )
            {
                reqValue = LL_DMA_MDATAALIGN_WORD;
            }
            else if( DMA_TRANSFER_SIZE_16BIT == memoryTransferSize )
            {
                reqValue = LL_DMA_MDATAALIGN_HALFWORD;
            }
            else
            {
                reqValue = LL_DMA_MDATAALIGN_BYTE;
            }

            LL_DMA_SetMemorySize( dma_PeriphConf[ dmaBus ].DmaReg,
                                  dma_PeriphConf[ dmaBus ].StreamsConfig[ dmaChannel ].StreamReg,
                                  reqValue );

            for( uint32_t iterationCnt = 0u; DMA_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
            {
                regValue = LL_DMA_GetMemorySize( dma_PeriphConf[ dmaBus ].DmaReg,
                                                 dma_PeriphConf[ dmaBus ].StreamsConfig[ dmaChannel ].StreamReg );

                if( reqValue == regValue )
                {
                    retState = DMA_REQUEST_OK;
                    break;
                }
                else
                {
                    /* Register has not yet been changed, keep return state as error */
                    retState = DMA_REQUEST_ERROR;
                }
            }
        }
    }
    else
    {
        retState = DMA_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Returns DMA memory transfer size.
 *
 * \param dmaBus              [in]: DMA peripheral ID
 * \param dmaChannel          [in]: DMA stream ID
 * \param memoryTransferSize [out]: Memory transfer size.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Get_MemoryTransferSize( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_TransferSize_t * const memoryTransferSize )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;
    uint32_t           regValue = 0u;

    if( ( DMA_PERIPH_CNT > dmaBus             ) &&
        ( DMA_STREAM_CNT > dmaChannel         ) &&
        ( DMA_NULL_PTR  != memoryTransferSize )    )
    {
        regValue = LL_DMA_GetMemorySize( dma_PeriphConf[ dmaBus ].DmaReg,
                                         dma_PeriphConf[ dmaBus ].StreamsConfig[ dmaChannel ].StreamReg );

        if( LL_DMA_MDATAALIGN_WORD == regValue )
        {
            *memoryTransferSize = DMA_TRANSFER_SIZE_32BIT;
        }
        else if( LL_DMA_MDATAALIGN_HALFWORD == regValue )
        {
            *memoryTransferSize = DMA_TRANSFER_SIZE_16BIT;
        }
        else
        {
            *memoryTransferSize = DMA_TRANSFER_SIZE_8BIT;
        }

        retState = DMA_REQUEST_OK;
    }
    else
    {
        retState = DMA_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Configures count of data to be transferred
 *
 * \param dmaBus     [in]: DMA peripheral ID
 * \param dmaChannel [in]: DMA stream ID
 * \param dataCount  [in]: Count of data to be transferred (max \ref DMA_DATA_COUNT_MAX)
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Set_DataCount( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_DataCount_t dataCount )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;
    uint32_t           regValue = 0u;

    if( ( DMA_PERIPH_CNT      > dmaBus     ) &&
        ( DMA_STREAM_CNT      > dmaChannel ) &&
        ( DMA_DATA_COUNT_MAX >= dataCount  )    )
    {
        const uint32_t streamEnabled = LL_DMA_IsEnabledStream( dma_PeriphConf[ dmaBus ].DmaReg,
                                                               dma_PeriphConf[ dmaBus ].StreamsConfig[ dmaChannel ].StreamReg );

        if( 0u != streamEnabled )
        {
            /* Configuration is write protected while the stream is enabled */
            retState = DMA_REQUEST_ERROR;
        }
        else
        {
            LL_DMA_SetDataLength( dma_PeriphConf[ dmaBus ].DmaReg,
                                  dma_PeriphConf[ dmaBus ].StreamsConfig[ dmaChannel ].StreamReg,
                                  dataCount );

            for( uint32_t iterationCnt = 0u; DMA_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
            {
                regValue = LL_DMA_GetDataLength( dma_PeriphConf[ dmaBus ].DmaReg,
                                                 dma_PeriphConf[ dmaBus ].StreamsConfig[ dmaChannel ].StreamReg );

                if( dataCount == regValue )
                {
                    retState = DMA_REQUEST_OK;
                    break;
                }
                else
                {
                    /* Register has not yet been changed, keep return state as error */
                    retState = DMA_REQUEST_ERROR;
                }
            }
        }
    }
    else
    {
        retState = DMA_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Returns count of data to be transferred
 *
 * \param dmaBus     [in]: DMA peripheral ID
 * \param dmaChannel [in]: DMA stream ID
 * \param dataCount [out]: Count of data to be transferred
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Get_DataCount( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_DataCount_t * const dataCount )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;

    if( ( DMA_PERIPH_CNT > dmaBus     ) &&
        ( DMA_STREAM_CNT > dmaChannel ) &&
        ( DMA_NULL_PTR  != dataCount  )    )
    {
        *dataCount = LL_DMA_GetDataLength( dma_PeriphConf[ dmaBus ].DmaReg,
                                           dma_PeriphConf[ dmaBus ].StreamsConfig[ dmaChannel ].StreamReg );

        retState = DMA_REQUEST_OK;
    }
    else
    {
        retState = DMA_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Configures DMA transfer trigger request ID (DMAMUX1 request of the stream)
 *
 * Request of UART / USART is served with bufferable transfers (device errata
 * "UART / USART - DMA transfer lock"), other requests without them.
 *
 * \param dmaBus     [in]: DMA peripheral ID
 * \param dmaChannel [in]: DMA stream ID
 * \param requestId  [in]: Identification of peripheral triggering transfer request
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Set_PeripheralRequest( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_PeriphReqId_t requestId )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;
    uint32_t           regValue = 0u;

    if( ( DMA_PERIPH_CNT > dmaBus                                ) &&
        ( DMA_STREAM_CNT > dmaChannel                            ) &&
        ( 0u == ( (uint32_t)requestId & ~DMAMUX_CxCR_DMAREQ_ID ) )    )
    {
        const uint32_t streamEnabled = LL_DMA_IsEnabledStream( dma_PeriphConf[ dmaBus ].DmaReg,
                                                               dma_PeriphConf[ dmaBus ].StreamsConfig[ dmaChannel ].StreamReg );

        if( 0u != streamEnabled )
        {
            /* Configuration is write protected while the stream is enabled */
            retState = DMA_REQUEST_ERROR;
        }
        else
        {
            LL_DMA_SetPeriphRequest( dma_PeriphConf[ dmaBus ].DmaReg,
                                     dma_PeriphConf[ dmaBus ].StreamsConfig[ dmaChannel ].StreamReg,
                                     requestId );

            for( uint32_t iterationCnt = 0u; DMA_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
            {
                regValue = LL_DMA_GetPeriphRequest( dma_PeriphConf[ dmaBus ].DmaReg,
                                                    dma_PeriphConf[ dmaBus ].StreamsConfig[ dmaChannel ].StreamReg );

                if( requestId == regValue )
                {
                    retState = DMA_REQUEST_OK;
                    break;
                }
                else
                {
                    /* Register has not yet been changed, keep return state as error */
                    retState = DMA_REQUEST_ERROR;
                }
            }

            if( DMA_REQUEST_OK == retState )
            {
                Dma_Set_BufferableTransfer( dmaBus, dmaChannel, requestId );
            }
            else
            {
                /* Request not applied - bufferable transfer configuration kept */
            }
        }
    }
    else
    {
        retState = DMA_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Returns DMA transfer trigger request ID (DMAMUX1 request of the stream)
 *
 * \param dmaBus      [in]: DMA peripheral ID
 * \param dmaChannel  [in]: DMA stream ID
 * \param requestId  [out]: Identification of peripheral triggering transfer request
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Get_PeripheralRequest( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_PeriphReqId_t * const requestId )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;

    if( ( DMA_PERIPH_CNT > dmaBus     ) &&
        ( DMA_STREAM_CNT > dmaChannel ) &&
        ( DMA_NULL_PTR  != requestId  )    )
    {
        *requestId = (dma_PeriphReqId_t)LL_DMA_GetPeriphRequest( dma_PeriphConf[ dmaBus ].DmaReg,
                                                                 dma_PeriphConf[ dmaBus ].StreamsConfig[ dmaChannel ].StreamReg );

        retState = DMA_REQUEST_OK;
    }
    else
    {
        retState = DMA_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Configures stream priority
 *
 * \param dmaBus     [in]: DMA peripheral ID
 * \param dmaChannel [in]: DMA stream ID
 * \param priority   [in]: Stream priority configuration
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Set_Priority( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_Priority_t priority )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;
    uint32_t           regValue = 0u;
    uint32_t           reqValue = 0u;

    if( ( DMA_PERIPH_CNT   > dmaBus     ) &&
        ( DMA_STREAM_CNT   > dmaChannel ) &&
        ( DMA_PRIORITY_CNT > priority   )    )
    {
        const uint32_t streamEnabled = LL_DMA_IsEnabledStream( dma_PeriphConf[ dmaBus ].DmaReg,
                                                               dma_PeriphConf[ dmaBus ].StreamsConfig[ dmaChannel ].StreamReg );

        if( 0u != streamEnabled )
        {
            /* Configuration is write protected while the stream is enabled */
            retState = DMA_REQUEST_ERROR;
        }
        else
        {
            if( DMA_PRIORITY_LOW == priority )
            {
                reqValue = LL_DMA_PRIORITY_LOW;
            }
            else if( DMA_PRIORITY_MEDIUM == priority )
            {
                reqValue = LL_DMA_PRIORITY_MEDIUM;
            }
            else if( DMA_PRIORITY_HIGH == priority )
            {
                reqValue = LL_DMA_PRIORITY_HIGH;
            }
            else
            {
                reqValue = LL_DMA_PRIORITY_VERYHIGH;
            }

            LL_DMA_SetStreamPriorityLevel( dma_PeriphConf[ dmaBus ].DmaReg,
                                           dma_PeriphConf[ dmaBus ].StreamsConfig[ dmaChannel ].StreamReg,
                                           reqValue );

            for( uint32_t iterationCnt = 0u; DMA_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
            {
                regValue = LL_DMA_GetStreamPriorityLevel( dma_PeriphConf[ dmaBus ].DmaReg,
                                                          dma_PeriphConf[ dmaBus ].StreamsConfig[ dmaChannel ].StreamReg );

                if( reqValue == regValue )
                {
                    retState = DMA_REQUEST_OK;
                    break;
                }
                else
                {
                    /* Register has not yet been changed, keep return state as error */
                    retState = DMA_REQUEST_ERROR;
                }
            }
        }
    }
    else
    {
        retState = DMA_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Returns stream priority
 *
 * \param dmaBus     [in]: DMA peripheral ID
 * \param dmaChannel [in]: DMA stream ID
 * \param priority  [out]: Stream priority configuration
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Get_Priority( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_Priority_t * const priority )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;
    uint32_t           regValue = 0u;

    if( ( DMA_PERIPH_CNT > dmaBus     ) &&
        ( DMA_STREAM_CNT > dmaChannel ) &&
        ( DMA_NULL_PTR  != priority   )    )
    {
        regValue = LL_DMA_GetStreamPriorityLevel( dma_PeriphConf[ dmaBus ].DmaReg,
                                                  dma_PeriphConf[ dmaBus ].StreamsConfig[ dmaChannel ].StreamReg );

        if( LL_DMA_PRIORITY_LOW == regValue )
        {
            *priority = DMA_PRIORITY_LOW;
        }
        else if( LL_DMA_PRIORITY_MEDIUM == regValue )
        {
            *priority = DMA_PRIORITY_MEDIUM;
        }
        else if( LL_DMA_PRIORITY_HIGH == regValue )
        {
            *priority = DMA_PRIORITY_HIGH;
        }
        else
        {
            *priority = DMA_PRIORITY_VERYHIGH;
        }

        retState = DMA_REQUEST_OK;
    }
    else
    {
        retState = DMA_REQUEST_ERROR;
    }

    return ( retState );
}

/*------------------------- Interrupts functionality -------------------------*/

/**
 * \brief Activates Interrupt Requests (IRQ) and configures Interrupt Service
 *        Routine (ISR) for selected stream.
 *
 * \param dmaBus     [in]: DMA peripheral ID
 * \param dmaChannel [in]: DMA stream ID
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Set_InterruptActive( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel )
{
    dma_RequestState_t  retState               = DMA_REQUEST_ERROR;
    nvic_RequestState_t nvicActivationState    = NVIC_REQUEST_ERROR;
    nvic_RequestState_t nvicHandlerConfigState = NVIC_REQUEST_ERROR;

    if( ( DMA_PERIPH_CNT > dmaBus     ) &&
        ( DMA_STREAM_CNT > dmaChannel )    )
    {
        nvicHandlerConfigState = Nvic_Set_PeriphIrq_Handler( dma_PeriphConf[ dmaBus ].StreamsConfig[ dmaChannel ].StreamIrq,
                                                             dma_PeriphConf[ dmaBus ].StreamsConfig[ dmaChannel ].StreamIsrHandler );

        nvicActivationState = Nvic_Set_PeriphIrq_Active( dma_PeriphConf[ dmaBus ].StreamsConfig[ dmaChannel ].StreamIrq );

        if( ( NVIC_REQUEST_OK != nvicActivationState    ) ||
            ( NVIC_REQUEST_OK != nvicHandlerConfigState )    )
        {
            retState  = DMA_REQUEST_ERROR;
        }
        else
        {
            retState  = DMA_REQUEST_OK;
        }
    }
    else
    {
        retState = DMA_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief De-activates Interrupt Requests (IRQ) of selected stream.
 *
 * \param dmaBus     [in]: DMA peripheral ID
 * \param dmaChannel [in]: DMA stream ID
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Set_InterruptInactive( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel )
{
    dma_RequestState_t  retState  = DMA_REQUEST_ERROR;
    nvic_RequestState_t nvicState = NVIC_REQUEST_ERROR;

    if( ( DMA_PERIPH_CNT > dmaBus     ) &&
        ( DMA_STREAM_CNT > dmaChannel )    )
    {
        nvicState = Nvic_Set_PeriphIrq_Inactive( dma_PeriphConf[ dmaBus ].StreamsConfig[ dmaChannel ].StreamIrq );

        if( NVIC_REQUEST_OK != nvicState )
        {
            retState  = DMA_REQUEST_ERROR;
        }
        else
        {
            retState  = DMA_REQUEST_OK;
        }
    }
    else
    {
        retState = DMA_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Enables transfer complete interrupt of selected stream.
 *
 * \param dmaBus     [in]: DMA peripheral ID
 * \param dmaChannel [in]: DMA stream ID
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Set_TransferCompleteIrqActive( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;
    uint32_t           regValue = 0u;

    if( ( DMA_PERIPH_CNT > dmaBus     ) &&
        ( DMA_STREAM_CNT > dmaChannel )    )
    {
        LL_DMA_EnableIT_TC( dma_PeriphConf[ dmaBus ].DmaReg,
                            dma_PeriphConf[ dmaBus ].StreamsConfig[ dmaChannel ].StreamReg );

        for( uint32_t iterationCnt = 0u; DMA_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
        {
            regValue = LL_DMA_IsEnabledIT_TC( dma_PeriphConf[ dmaBus ].DmaReg,
                                              dma_PeriphConf[ dmaBus ].StreamsConfig[ dmaChannel ].StreamReg );

            if( 0u != regValue )
            {
                retState = DMA_REQUEST_OK;
                break;
            }
            else
            {
                /* Register has not yet been changed, keep return state as error */
                retState = DMA_REQUEST_ERROR;
            }
        }
    }
    else
    {
        retState = DMA_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Disables transfer complete interrupt of selected stream.
 *
 * \param dmaBus     [in]: DMA peripheral ID
 * \param dmaChannel [in]: DMA stream ID
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Set_TransferCompleteIrqInactive( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;
    uint32_t           regValue = 0u;

    if( ( DMA_PERIPH_CNT > dmaBus     ) &&
        ( DMA_STREAM_CNT > dmaChannel )    )
    {
        LL_DMA_DisableIT_TC( dma_PeriphConf[ dmaBus ].DmaReg,
                             dma_PeriphConf[ dmaBus ].StreamsConfig[ dmaChannel ].StreamReg );

        for( uint32_t iterationCnt = 0u; DMA_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
        {
            regValue = LL_DMA_IsEnabledIT_TC( dma_PeriphConf[ dmaBus ].DmaReg,
                                              dma_PeriphConf[ dmaBus ].StreamsConfig[ dmaChannel ].StreamReg );

            if( 0u == regValue )
            {
                retState = DMA_REQUEST_OK;
                break;
            }
            else
            {
                /* Register has not yet been changed, keep return state as error */
                retState = DMA_REQUEST_ERROR;
            }
        }
    }
    else
    {
        retState = DMA_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Enables half transfer interrupt of selected stream.
 *
 * \param dmaBus     [in]: DMA peripheral ID
 * \param dmaChannel [in]: DMA stream ID
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Set_HalfTransferIrqActive( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;
    uint32_t           regValue = 0u;

    if( ( DMA_PERIPH_CNT > dmaBus     ) &&
        ( DMA_STREAM_CNT > dmaChannel )    )
    {
        LL_DMA_EnableIT_HT( dma_PeriphConf[ dmaBus ].DmaReg,
                            dma_PeriphConf[ dmaBus ].StreamsConfig[ dmaChannel ].StreamReg );

        for( uint32_t iterationCnt = 0u; DMA_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
        {
            regValue = LL_DMA_IsEnabledIT_HT( dma_PeriphConf[ dmaBus ].DmaReg,
                                              dma_PeriphConf[ dmaBus ].StreamsConfig[ dmaChannel ].StreamReg );

            if( 0u != regValue )
            {
                retState = DMA_REQUEST_OK;
                break;
            }
            else
            {
                /* Register has not yet been changed, keep return state as error */
                retState = DMA_REQUEST_ERROR;
            }
        }
    }
    else
    {
        retState = DMA_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Disables half transfer interrupt of selected stream.
 *
 * \param dmaBus     [in]: DMA peripheral ID
 * \param dmaChannel [in]: DMA stream ID
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Set_HalfTransferIrqInactive( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;
    uint32_t           regValue = 0u;

    if( ( DMA_PERIPH_CNT > dmaBus     ) &&
        ( DMA_STREAM_CNT > dmaChannel )    )
    {
        LL_DMA_DisableIT_HT( dma_PeriphConf[ dmaBus ].DmaReg,
                             dma_PeriphConf[ dmaBus ].StreamsConfig[ dmaChannel ].StreamReg );

        for( uint32_t iterationCnt = 0u; DMA_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
        {
            regValue = LL_DMA_IsEnabledIT_HT( dma_PeriphConf[ dmaBus ].DmaReg,
                                              dma_PeriphConf[ dmaBus ].StreamsConfig[ dmaChannel ].StreamReg );

            if( 0u == regValue )
            {
                retState = DMA_REQUEST_OK;
                break;
            }
            else
            {
                /* Register has not yet been changed, keep return state as error */
                retState = DMA_REQUEST_ERROR;
            }
        }
    }
    else
    {
        retState = DMA_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Enables transfer error interrupt of selected stream.
 *
 * \param dmaBus     [in]: DMA peripheral ID
 * \param dmaChannel [in]: DMA stream ID
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Set_TransferErrorIrqActive( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;
    uint32_t           regValue = 0u;

    if( ( DMA_PERIPH_CNT > dmaBus     ) &&
        ( DMA_STREAM_CNT > dmaChannel )    )
    {
        LL_DMA_EnableIT_TE( dma_PeriphConf[ dmaBus ].DmaReg,
                            dma_PeriphConf[ dmaBus ].StreamsConfig[ dmaChannel ].StreamReg );

        for( uint32_t iterationCnt = 0u; DMA_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
        {
            regValue = LL_DMA_IsEnabledIT_TE( dma_PeriphConf[ dmaBus ].DmaReg,
                                              dma_PeriphConf[ dmaBus ].StreamsConfig[ dmaChannel ].StreamReg );

            if( 0u != regValue )
            {
                retState = DMA_REQUEST_OK;
                break;
            }
            else
            {
                /* Register has not yet been changed, keep return state as error */
                retState = DMA_REQUEST_ERROR;
            }
        }
    }
    else
    {
        retState = DMA_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Disables transfer error interrupt of selected stream.
 *
 * \param dmaBus     [in]: DMA peripheral ID
 * \param dmaChannel [in]: DMA stream ID
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Set_TransferErrorIrqInactive( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;
    uint32_t           regValue = 0u;

    if( ( DMA_PERIPH_CNT > dmaBus     ) &&
        ( DMA_STREAM_CNT > dmaChannel )    )
    {
        LL_DMA_DisableIT_TE( dma_PeriphConf[ dmaBus ].DmaReg,
                             dma_PeriphConf[ dmaBus ].StreamsConfig[ dmaChannel ].StreamReg );

        for( uint32_t iterationCnt = 0u; DMA_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
        {
            regValue = LL_DMA_IsEnabledIT_TE( dma_PeriphConf[ dmaBus ].DmaReg,
                                              dma_PeriphConf[ dmaBus ].StreamsConfig[ dmaChannel ].StreamReg );

            if( 0u == regValue )
            {
                retState = DMA_REQUEST_OK;
                break;
            }
            else
            {
                /* Register has not yet been changed, keep return state as error */
                retState = DMA_REQUEST_ERROR;
            }
        }
    }
    else
    {
        retState = DMA_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Configures transfer complete callback of selected stream.
 *
 * \param dmaBus     [in]: DMA peripheral ID
 * \param dmaChannel [in]: DMA stream ID
 * \param irqHandler [in]: Callback (null pointer disables the callback)
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Set_TransferCompleteIsrHandler( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_IsrCallback irqHandler )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;

    if( ( DMA_PERIPH_CNT > dmaBus     ) &&
        ( DMA_STREAM_CNT > dmaChannel )    )
    {
        dma_PeriphConf[ dmaBus ].IsrCallbacks[ dmaChannel ].TransferCompleteCallback = irqHandler;

        retState = DMA_REQUEST_OK;
    }
    else
    {
        retState = DMA_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Returns transfer complete callback of selected stream.
 *
 * \param dmaBus      [in]: DMA peripheral ID
 * \param dmaChannel  [in]: DMA stream ID
 * \param irqHandler [out]: Callback
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Get_TransferCompleteIsrHandler( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_IsrCallback * const irqHandler )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;

    if( ( DMA_PERIPH_CNT > dmaBus     ) &&
        ( DMA_STREAM_CNT > dmaChannel ) &&
        ( DMA_NULL_PTR  != irqHandler )    )
    {
        *irqHandler = dma_PeriphConf[ dmaBus ].IsrCallbacks[ dmaChannel ].TransferCompleteCallback;

        retState = DMA_REQUEST_OK;
    }
    else
    {
        retState = DMA_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Configures half transfer callback of selected stream.
 *
 * \param dmaBus     [in]: DMA peripheral ID
 * \param dmaChannel [in]: DMA stream ID
 * \param irqHandler [in]: Callback (null pointer disables the callback)
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Set_HalfTransferIsrHandler( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_IsrCallback irqHandler )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;

    if( ( DMA_PERIPH_CNT > dmaBus     ) &&
        ( DMA_STREAM_CNT > dmaChannel )    )
    {
        dma_PeriphConf[ dmaBus ].IsrCallbacks[ dmaChannel ].HalfTransferCallback = irqHandler;

        retState = DMA_REQUEST_OK;
    }
    else
    {
        retState = DMA_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Returns half transfer callback of selected stream.
 *
 * \param dmaBus      [in]: DMA peripheral ID
 * \param dmaChannel  [in]: DMA stream ID
 * \param irqHandler [out]: Callback
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Get_HalfTransferIsrHandler( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_IsrCallback * const irqHandler )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;

    if( ( DMA_PERIPH_CNT > dmaBus     ) &&
        ( DMA_STREAM_CNT > dmaChannel ) &&
        ( DMA_NULL_PTR  != irqHandler )    )
    {
        *irqHandler = dma_PeriphConf[ dmaBus ].IsrCallbacks[ dmaChannel ].HalfTransferCallback;

        retState = DMA_REQUEST_OK;
    }
    else
    {
        retState = DMA_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Configures transfer error callback of selected stream.
 *
 * \param dmaBus     [in]: DMA peripheral ID
 * \param dmaChannel [in]: DMA stream ID
 * \param irqHandler [in]: Callback (null pointer disables the callback)
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Set_TransferErrorIsrHandler( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_IsrCallback irqHandler )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;

    if( ( DMA_PERIPH_CNT > dmaBus     ) &&
        ( DMA_STREAM_CNT > dmaChannel )    )
    {
        dma_PeriphConf[ dmaBus ].IsrCallbacks[ dmaChannel ].TransferErrorCallback = irqHandler;

        retState = DMA_REQUEST_OK;
    }
    else
    {
        retState = DMA_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Returns transfer error callback of selected stream.
 *
 * \param dmaBus      [in]: DMA peripheral ID
 * \param dmaChannel  [in]: DMA stream ID
 * \param irqHandler [out]: Callback
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Get_TransferErrorIsrHandler( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_IsrCallback * const irqHandler )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;

    if( ( DMA_PERIPH_CNT > dmaBus     ) &&
        ( DMA_STREAM_CNT > dmaChannel ) &&
        ( DMA_NULL_PTR  != irqHandler )    )
    {
        *irqHandler = dma_PeriphConf[ dmaBus ].IsrCallbacks[ dmaChannel ].TransferErrorCallback;

        retState = DMA_REQUEST_OK;
    }
    else
    {
        retState = DMA_REQUEST_ERROR;
    }

    return ( retState );
}

/* =========================== LOCAL FUNCTIONS ============================== */

/**
 * \brief Returns flags of selected stream aligned to stream 0 position
 *        (\ref DMA_FLAG_TC, \ref DMA_FLAG_HT, \ref DMA_FLAG_TE, ...)
 *
 * \param dmaBus     [in]: DMA peripheral ID
 * \param dmaChannel [in]: DMA stream ID
 * \return Active flags of the stream
 */
static uint32_t Dma_Get_Flags( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel )
{
    uint32_t regValue = 0u;

    if( DMA_ISR_STREAM_CNT > (uint32_t)dmaChannel )
    {
        regValue = READ_REG( dma_PeriphConf[ dmaBus ].DmaReg->LISR );
    }
    else
    {
        regValue = READ_REG( dma_PeriphConf[ dmaBus ].DmaReg->HISR );
    }

    return ( ( regValue >> dma_FlagOffsetLut[ (uint32_t)dmaChannel % DMA_ISR_STREAM_CNT ] ) & DMA_FLAG_ALL );
}


/**
 * \brief Clears flags of selected stream
 *
 * \param dmaBus     [in]: DMA peripheral ID
 * \param dmaChannel [in]: DMA stream ID
 * \param flags      [in]: Flags aligned to stream 0 position (\ref DMA_FLAG_ALL)
 */
static void Dma_Clear_Flags( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, uint32_t flags )
{
    uint32_t clearMask = ( flags & DMA_FLAG_ALL ) << dma_FlagOffsetLut[ (uint32_t)dmaChannel % DMA_ISR_STREAM_CNT ];

    if( DMA_ISR_STREAM_CNT > (uint32_t)dmaChannel )
    {
        WRITE_REG( dma_PeriphConf[ dmaBus ].DmaReg->LIFCR, clearMask );
    }
    else
    {
        WRITE_REG( dma_PeriphConf[ dmaBus ].DmaReg->HIFCR, clearMask );
    }
}


/**
 * \brief Configures FIFO of selected stream (FIFO inactive - direct mode).
 *
 * FIFO threshold is set to full FIFO, which is valid for single (non burst)
 * transfers of all data sizes.
 *
 * \param dmaBus     [in]: DMA peripheral ID
 * \param dmaChannel [in]: DMA stream ID
 * \param fifoState  [in]: FIFO activation state
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
static dma_RequestState_t Dma_Set_FifoMode( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_FunctionState_t fifoState )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;
    uint32_t           regValue = 0u;
    uint32_t           reqValue = 0u;

    const uint32_t streamEnabled = LL_DMA_IsEnabledStream( dma_PeriphConf[ dmaBus ].DmaReg,
                                                           dma_PeriphConf[ dmaBus ].StreamsConfig[ dmaChannel ].StreamReg );

    if( 0u != streamEnabled )
    {
        /* Configuration is write protected while the stream is enabled */
        retState = DMA_REQUEST_ERROR;
    }
    else
    {
        if( DMA_FUNCTION_ACTIVE == fifoState )
        {
            reqValue = LL_DMA_FIFOMODE_ENABLE;
        }
        else
        {
            reqValue = LL_DMA_FIFOMODE_DISABLE;
        }

        LL_DMA_ConfigFifo( dma_PeriphConf[ dmaBus ].DmaReg,
                           dma_PeriphConf[ dmaBus ].StreamsConfig[ dmaChannel ].StreamReg,
                           reqValue,
                           LL_DMA_FIFOTHRESHOLD_FULL );

        for( uint32_t iterationCnt = 0u; DMA_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
        {
            regValue = READ_BIT( DMA_STREAM_REG( dmaBus, dmaChannel )->FCR, DMA_SxFCR_DMDIS );

            if( reqValue == regValue )
            {
                retState = DMA_REQUEST_OK;
                break;
            }
            else
            {
                /* Register has not yet been changed, keep return state as error */
                retState = DMA_REQUEST_ERROR;
            }
        }
    }

    return ( retState );
}

/* =========================== INTERRUPT HANDLERS =========================== */

/**
 * \brief Checks if the request belongs to UART / USART (bufferable transfers).
 *
 * \param requestId [in]: DMAMUX1 request identification
 * \return \ref DMA_FUNCTION_ACTIVE for UART / USART request, otherwise
 *         \ref DMA_FUNCTION_INACTIVE.
 */
static dma_FunctionState_t Dma_Get_UartRequest( dma_PeriphReqId_t requestId )
{
    dma_FunctionState_t retState = DMA_FUNCTION_INACTIVE;

    for( uint32_t lutIdx = 0u; ( sizeof( dma_UartRequestLut ) / sizeof( dma_UartRequestLut[ 0u ] ) ) > lutIdx; lutIdx ++ )
    {
        if( dma_UartRequestLut[ lutIdx ] == requestId )
        {
            retState = DMA_FUNCTION_ACTIVE;
            break;
        }
        else
        {
            /* Other request */
        }
    }

    return ( retState );
}


/**
 * \brief Configures bufferable transfers (SxCR TRBUFF) of the stream according to
 *        the request.
 *
 * Device errata "UART / USART - DMA transfer lock": DMA stream serving UART / USART
 * may be locked - bufferable transfers are used for UART / USART requests (as ST
 * HAL does). Revision Y of STM32H742 / H743 / H745 / H747 / H750 / H753 / H755 /
 * H757 does not use them (ST HAL).
 *
 * \param dmaBus     [in]: DMA peripheral ID
 * \param dmaChannel [in]: DMA stream ID
 * \param requestId  [in]: DMAMUX1 request identification
 */
static void Dma_Set_BufferableTransfer( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_PeriphReqId_t requestId )
{
    dma_FunctionState_t bufferable = Dma_Get_UartRequest( requestId );

#if (STM32H7_DEV_ID == 0x450UL)
    if( DMA_DEV_REV_ID_V > ( DBGMCU->IDCODE & DBGMCU_IDCODE_REV_ID ) )
    {
        /* Revision Y - bufferable transfers are not used */
        bufferable = DMA_FUNCTION_INACTIVE;
    }
    else
    {
        /* Revision V and newer */
    }
#endif

    if( DMA_FUNCTION_ACTIVE == bufferable )
    {
        LL_DMA_EnableBufferableTransfer( dma_PeriphConf[ dmaBus ].DmaReg,
                                         dma_PeriphConf[ dmaBus ].StreamsConfig[ dmaChannel ].StreamReg );
    }
    else
    {
        LL_DMA_DisableBufferableTransfer( dma_PeriphConf[ dmaBus ].DmaReg,
                                          dma_PeriphConf[ dmaBus ].StreamsConfig[ dmaChannel ].StreamReg );
    }
}


/**
 * \brief Common interrupt handler of DMA streams.
 *
 * Flag is cleared and callback executed only for interrupts enabled on the
 * stream. Half transfer is processed before transfer complete (both flags can
 * be active in one interrupt of short transfer).
 *
 * \param dmaBus     [in]: DMA peripheral ID
 * \param dmaChannel [in]: DMA stream ID
 */
static void Dma_GlobalIsrHandler( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel )
{
    uint32_t       flagStatus = Dma_Get_Flags( dmaBus, dmaChannel );
    const uint32_t htIrqState = LL_DMA_IsEnabledIT_HT( dma_PeriphConf[ dmaBus ].DmaReg,
                                                       dma_PeriphConf[ dmaBus ].StreamsConfig[ dmaChannel ].StreamReg );
    const uint32_t tcIrqState = LL_DMA_IsEnabledIT_TC( dma_PeriphConf[ dmaBus ].DmaReg,
                                                       dma_PeriphConf[ dmaBus ].StreamsConfig[ dmaChannel ].StreamReg );
    const uint32_t teIrqState = LL_DMA_IsEnabledIT_TE( dma_PeriphConf[ dmaBus ].DmaReg,
                                                       dma_PeriphConf[ dmaBus ].StreamsConfig[ dmaChannel ].StreamReg );

    if( ( 0u != ( DMA_FLAG_HT & flagStatus ) ) &&
        ( 0u != htIrqState                   )    )
    {
        Dma_Clear_Flags( dmaBus, dmaChannel, DMA_FLAG_HT );

        if( DMA_NULL_PTR != dma_PeriphConf[ dmaBus ].IsrCallbacks[ dmaChannel ].HalfTransferCallback )
        {
            dma_PeriphConf[ dmaBus ].IsrCallbacks[ dmaChannel ].HalfTransferCallback();
        }
        else
        {
            /* Interrupt callback is not configured */
        }
    }
    else
    {
        /* No action required */
    }

    if( ( 0u != ( DMA_FLAG_TC & flagStatus ) ) &&
        ( 0u != tcIrqState                   )    )
    {
        Dma_Clear_Flags( dmaBus, dmaChannel, DMA_FLAG_TC );

        if( DMA_NULL_PTR != dma_PeriphConf[ dmaBus ].IsrCallbacks[ dmaChannel ].TransferCompleteCallback )
        {
            dma_PeriphConf[ dmaBus ].IsrCallbacks[ dmaChannel ].TransferCompleteCallback();
        }
        else
        {
            /* Interrupt callback is not configured */
        }
    }
    else
    {
        /* No action required */
    }

    if( ( 0u != ( DMA_FLAG_TE & flagStatus ) ) &&
        ( 0u != teIrqState                   )    )
    {
        Dma_Clear_Flags( dmaBus, dmaChannel, DMA_FLAG_TE );

        if( DMA_NULL_PTR != dma_PeriphConf[ dmaBus ].IsrCallbacks[ dmaChannel ].TransferErrorCallback )
        {
            dma_PeriphConf[ dmaBus ].IsrCallbacks[ dmaChannel ].TransferErrorCallback();
        }
        else
        {
            /* Interrupt callback is not configured */
        }
    }
    else
    {
        /* No action required */
    }
}


static void Dma_Dma1Stream0_IsrHandler( void )
{
    Dma_GlobalIsrHandler( DMA_PERIPH_1, DMA_STREAM_0 );
}


static void Dma_Dma1Stream1_IsrHandler( void )
{
    Dma_GlobalIsrHandler( DMA_PERIPH_1, DMA_STREAM_1 );
}


static void Dma_Dma1Stream2_IsrHandler( void )
{
    Dma_GlobalIsrHandler( DMA_PERIPH_1, DMA_STREAM_2 );
}


static void Dma_Dma1Stream3_IsrHandler( void )
{
    Dma_GlobalIsrHandler( DMA_PERIPH_1, DMA_STREAM_3 );
}


static void Dma_Dma1Stream4_IsrHandler( void )
{
    Dma_GlobalIsrHandler( DMA_PERIPH_1, DMA_STREAM_4 );
}


static void Dma_Dma1Stream5_IsrHandler( void )
{
    Dma_GlobalIsrHandler( DMA_PERIPH_1, DMA_STREAM_5 );
}


static void Dma_Dma1Stream6_IsrHandler( void )
{
    Dma_GlobalIsrHandler( DMA_PERIPH_1, DMA_STREAM_6 );
}


static void Dma_Dma1Stream7_IsrHandler( void )
{
    Dma_GlobalIsrHandler( DMA_PERIPH_1, DMA_STREAM_7 );
}


#if defined(DMA2)
static void Dma_Dma2Stream0_IsrHandler( void )
{
    Dma_GlobalIsrHandler( DMA_PERIPH_2, DMA_STREAM_0 );
}


static void Dma_Dma2Stream1_IsrHandler( void )
{
    Dma_GlobalIsrHandler( DMA_PERIPH_2, DMA_STREAM_1 );
}


static void Dma_Dma2Stream2_IsrHandler( void )
{
    Dma_GlobalIsrHandler( DMA_PERIPH_2, DMA_STREAM_2 );
}


static void Dma_Dma2Stream3_IsrHandler( void )
{
    Dma_GlobalIsrHandler( DMA_PERIPH_2, DMA_STREAM_3 );
}


static void Dma_Dma2Stream4_IsrHandler( void )
{
    Dma_GlobalIsrHandler( DMA_PERIPH_2, DMA_STREAM_4 );
}


static void Dma_Dma2Stream5_IsrHandler( void )
{
    Dma_GlobalIsrHandler( DMA_PERIPH_2, DMA_STREAM_5 );
}


static void Dma_Dma2Stream6_IsrHandler( void )
{
    Dma_GlobalIsrHandler( DMA_PERIPH_2, DMA_STREAM_6 );
}


static void Dma_Dma2Stream7_IsrHandler( void )
{
    Dma_GlobalIsrHandler( DMA_PERIPH_2, DMA_STREAM_7 );
}

#endif

/* ================================ TASKS =================================== */

#endif /* DMA1 */
