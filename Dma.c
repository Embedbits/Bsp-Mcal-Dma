/**
 * \author Mr.Nobody
 * \file Dma.h
 * \ingroup Dma
 * \brief Dma module common functionality
 *
 */
/* ============================== INCLUDES ================================== */
#include "Dma.h"                            /* Self include                   */
#include "Dma_Port.h"                       /* Own port file include          */
#include "Dma_Types.h"                      /* Module types definitions       */
#include "Stm32_dma.h"                      /* DMA Registers definitions      */
#include "Rcc_Port.h"                       /* RCC module functionality       */
#include "Nvic_Port.h"                      /* NVIC module functionality      */
#include "Stm32_utils.h"                    /* STM32 utilities module         */
#include "Stm32_bus.h"                      /* STM32 bus module               */
/* ========================== SYMBOLIC CONSTANTS ============================ */

/** Value of major version of SW module */
#define DMA_MAJOR_VERSION           ( 1u )

/** Value of minor version of SW module */
#define DMA_MINOR_VERSION           ( 0u )

/** Value of patch version of SW module */
#define DMA_PATCH_VERSION           ( 0u )

/** Maximum timeout used for waiting for register update verification */
#define DMA_TIMEOUT_RAW             ( 0x84FCB )

/* =============================== MACROS =================================== */

/** Global Interrupt Request (IRQ) flag identification for use in \ref DMA_GET_ACTIVE_IRQ_FLAG */
#define DMA_GI_IRQ_FLAG                                 ( 0u )
/** Transfer complete Interrupt Request (IRQ) flag identification for use in \ref DMA_GET_ACTIVE_IRQ_FLAG */
#define DMA_TC_IRQ_FLAG                                 ( 1u )
/** Half transfer Interrupt Request (IRQ) flag identification for use in \ref DMA_GET_ACTIVE_IRQ_FLAG */
#define DMA_HT_IRQ_FLAG                                 ( 2u )
/** Transfer error Interrupt Request (IRQ) flag identification for use in \ref DMA_GET_ACTIVE_IRQ_FLAG */
#define DMA_TE_IRQ_FLAG                                 ( 3u )
/**
 * \brief Multiplier for use in \ref DMA_GET_ACTIVE_IRQ_FLAG
 *
 *  Multiplier value represents count of bits used for every channel in
 *  interrupt status register. Every channel has interrupt flags in same order.
 *  The reading of flag bit is done as masking by bit shifted by channel ID
 *  (0-6 for channels 1-7) multiplied by offset (count of status bits per channel)
 *  and incremented by flag ID (offset of bit within channel)
 */
#define DMA_IRQ_MULTIPLIER                              ( 4u )

/**
 * \brief Macro reading interrupt status flag.
 *
 *  Multiplier value represents count of bits used for every channel in
 *  interrupt status register. Every channel has interrupt flags in same order.
 *  The reading of flag bit is done as masking by bit shifted by channel ID
 *  (0-6 for channels 1-7) multiplied by offset (count of status bits per channel)
 *  and incremented by flag ID (offset of bit within channel)
 *
 *  Parameter DMAx is representing DMA peripheral, CHANNEL is representing
 *  channel id from \ref dma_ChannelId_t and flag can be one of
 *  \ref DMA_GI_IRQ_FLAG for channel global interrupt
 *  \ref DMA_TC_IRQ_FLAG for transfer complete flag
 *  \ref DMA_HT_IRQ_FLAG for half transfer flag
 *  \ref DMA_TE_IRQ_FLAG for transfer error flag
 */
#define DMA_GET_ACTIVE_IRQ_FLAG(DMAx, CHANNEL, FLAG)    ( READ_BIT( DMAx->ISR, ( 0x01 << ( ( CHANNEL * DMA_IRQ_MULTIPLIER ) + FLAG ) ) ) )

/**
 * \brief Macro clearing interrupt status flag.
 *
 *  Multiplier value represents count of bits used for every channel in
 *  interrupt status register. Every channel has interrupt flags in same order.
 *  The reading of flag bit is done as masking by bit shifted by channel ID
 *  (0-6 for channels 1-7) multiplied by offset (count of status bits per channel)
 *  and incremented by flag ID (offset of bit within channel)
 *
 *  Parameter DMAx is representing DMA peripheral, CHANNEL is representing
 *  channel id from \ref dma_ChannelId_t and flag can be one of
 *  \ref DMA_GI_IRQ_FLAG for channel global interrupt
 *  \ref DMA_TC_IRQ_FLAG for transfer complete flag
 *  \ref DMA_HT_IRQ_FLAG for half transfer flag
 *  \ref DMA_TE_IRQ_FLAG for transfer error flag
 */
#define DMA_CLEAR_ACTIVE_IRQ_FLAG(DMAx, CHANNEL, FLAG)  ( CLEAR_BIT( DMAx->IFCR, ( 0x01 << ( ( CHANNEL * DMA_IRQ_MULTIPLIER ) + FLAG ) ) ) )

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

/** \brief Structure type used for storing channels configuration */
typedef struct
{
    dma_ChannelId_t      ChannelId;        /**< Channel identification                               */
    nvic_PeriphIrqList_t ChannelIrq;       /**< Channel interrupt identification used by NVIC module */
    uint32_t             ChannelReg;       /**< Channel identification used for register operations  */
    dma_IsrHandler_t     ChannelIsrHandler;/**< Channel Interrupt Status Routine (ISR) handler       */
}   dma_ChannelsConfig_t;

/** \brief Structure type used by DMA configuration array */
typedef struct
{
          dma_PeriphId_t        DmaPeriphId;           /**< DMA bus identification     */
          rcc_PeriphId_t        RccPeriphId;           /**< RCC bus identification     */
          DMA_TypeDef          *DmaReg;                /**< DMA bus register address   */
    const dma_ChannelsConfig_t * const ChannelsConfig; /**< DMA channels configuration */
    volatile dma_IsrCallbacks_t* const IsrCallbacks;   /**< ISR callback's array       */
}   dma_PeriphConfig_t;

/* ======================== FORWARD DECLARATIONS ============================ */

static void Dma_GlobalIsrHandler( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel );

static void Dma_Dma1Channel1_IsrHandler( void );
static void Dma_Dma1Channel2_IsrHandler( void );
static void Dma_Dma1Channel3_IsrHandler( void );
static void Dma_Dma1Channel4_IsrHandler( void );
static void Dma_Dma1Channel5_IsrHandler( void );
static void Dma_Dma1Channel6_IsrHandler( void );
static void Dma_Dma1Channel7_IsrHandler( void );
#if defined(DMA2)
static void Dma_Dma2Channel1_IsrHandler( void );
static void Dma_Dma2Channel2_IsrHandler( void );
static void Dma_Dma2Channel3_IsrHandler( void );
static void Dma_Dma2Channel4_IsrHandler( void );
static void Dma_Dma2Channel5_IsrHandler( void );
static void Dma_Dma2Channel6_IsrHandler( void );
static void Dma_Dma2Channel7_IsrHandler( void );
#endif

/* ========================== EXPORTED VARIABLES ============================ */

/* =========================== LOCAL VARIABLES ============================== */

static volatile dma_IsrCallbacks_t          dma_Dma1IrqCallbacks[ DMA_CHANNEL_CNT ] =
{
    { .TransferCompleteCallback = DMA_NULL_PTR , .HalfTransferCallback = DMA_NULL_PTR , .TransferErrorCallback = DMA_NULL_PTR },
    { .TransferCompleteCallback = DMA_NULL_PTR , .HalfTransferCallback = DMA_NULL_PTR , .TransferErrorCallback = DMA_NULL_PTR },
    { .TransferCompleteCallback = DMA_NULL_PTR , .HalfTransferCallback = DMA_NULL_PTR , .TransferErrorCallback = DMA_NULL_PTR },
    { .TransferCompleteCallback = DMA_NULL_PTR , .HalfTransferCallback = DMA_NULL_PTR , .TransferErrorCallback = DMA_NULL_PTR },
    { .TransferCompleteCallback = DMA_NULL_PTR , .HalfTransferCallback = DMA_NULL_PTR , .TransferErrorCallback = DMA_NULL_PTR },
    { .TransferCompleteCallback = DMA_NULL_PTR , .HalfTransferCallback = DMA_NULL_PTR , .TransferErrorCallback = DMA_NULL_PTR },
    { .TransferCompleteCallback = DMA_NULL_PTR , .HalfTransferCallback = DMA_NULL_PTR , .TransferErrorCallback = DMA_NULL_PTR }
};

#if defined(DMA2)
static volatile dma_IsrCallbacks_t          dma_Dma2IrqCallbacks[ DMA_CHANNEL_CNT ] =
{
    { .TransferCompleteCallback = DMA_NULL_PTR , .HalfTransferCallback = DMA_NULL_PTR , .TransferErrorCallback = DMA_NULL_PTR },
    { .TransferCompleteCallback = DMA_NULL_PTR , .HalfTransferCallback = DMA_NULL_PTR , .TransferErrorCallback = DMA_NULL_PTR },
    { .TransferCompleteCallback = DMA_NULL_PTR , .HalfTransferCallback = DMA_NULL_PTR , .TransferErrorCallback = DMA_NULL_PTR },
    { .TransferCompleteCallback = DMA_NULL_PTR , .HalfTransferCallback = DMA_NULL_PTR , .TransferErrorCallback = DMA_NULL_PTR },
    { .TransferCompleteCallback = DMA_NULL_PTR , .HalfTransferCallback = DMA_NULL_PTR , .TransferErrorCallback = DMA_NULL_PTR },
    { .TransferCompleteCallback = DMA_NULL_PTR , .HalfTransferCallback = DMA_NULL_PTR , .TransferErrorCallback = DMA_NULL_PTR },
    { .TransferCompleteCallback = DMA_NULL_PTR , .HalfTransferCallback = DMA_NULL_PTR , .TransferErrorCallback = DMA_NULL_PTR }
};
#endif

static const dma_ChannelsConfig_t           dma_Dma1IrqConfig[ DMA_CHANNEL_CNT ] =
{
    { .ChannelId = DMA_CHANNEL_1 , .ChannelIrq = NVIC_PERIPH_IRQ_DMA1_CHANNEL1 , .ChannelReg = LL_DMA_CHANNEL_1 , .ChannelIsrHandler = Dma_Dma1Channel1_IsrHandler },
    { .ChannelId = DMA_CHANNEL_2 , .ChannelIrq = NVIC_PERIPH_IRQ_DMA1_CHANNEL2 , .ChannelReg = LL_DMA_CHANNEL_2 , .ChannelIsrHandler = Dma_Dma1Channel2_IsrHandler },
    { .ChannelId = DMA_CHANNEL_3 , .ChannelIrq = NVIC_PERIPH_IRQ_DMA1_CHANNEL3 , .ChannelReg = LL_DMA_CHANNEL_3 , .ChannelIsrHandler = Dma_Dma1Channel3_IsrHandler },
    { .ChannelId = DMA_CHANNEL_4 , .ChannelIrq = NVIC_PERIPH_IRQ_DMA1_CHANNEL4 , .ChannelReg = LL_DMA_CHANNEL_4 , .ChannelIsrHandler = Dma_Dma1Channel4_IsrHandler },
    { .ChannelId = DMA_CHANNEL_5 , .ChannelIrq = NVIC_PERIPH_IRQ_DMA1_CHANNEL5 , .ChannelReg = LL_DMA_CHANNEL_5 , .ChannelIsrHandler = Dma_Dma1Channel5_IsrHandler },
    { .ChannelId = DMA_CHANNEL_6 , .ChannelIrq = NVIC_PERIPH_IRQ_DMA1_CHANNEL6 , .ChannelReg = LL_DMA_CHANNEL_6 , .ChannelIsrHandler = Dma_Dma1Channel6_IsrHandler },
    { .ChannelId = DMA_CHANNEL_7 , .ChannelIrq = NVIC_PERIPH_IRQ_DMA1_CHANNEL7 , .ChannelReg = LL_DMA_CHANNEL_7 , .ChannelIsrHandler = Dma_Dma1Channel7_IsrHandler },
};

#if defined(DMA2)
static const dma_ChannelsConfig_t           dma_Dma2IrqConfig[ DMA_CHANNEL_CNT ] =
{
    { .ChannelId = DMA_CHANNEL_1 , .ChannelIrq = NVIC_PERIPH_IRQ_DMA2_CHANNEL1  , .ChannelReg = LL_DMA_CHANNEL_1 , .ChannelIsrHandler = Dma_Dma2Channel1_IsrHandler },
    { .ChannelId = DMA_CHANNEL_2 , .ChannelIrq = NVIC_PERIPH_IRQ_DMA2_CHANNEL2  , .ChannelReg = LL_DMA_CHANNEL_2 , .ChannelIsrHandler = Dma_Dma2Channel2_IsrHandler },
    { .ChannelId = DMA_CHANNEL_3 , .ChannelIrq = NVIC_PERIPH_IRQ_DMA2_CHANNEL3  , .ChannelReg = LL_DMA_CHANNEL_3 , .ChannelIsrHandler = Dma_Dma2Channel3_IsrHandler },
    { .ChannelId = DMA_CHANNEL_4 , .ChannelIrq = NVIC_PERIPH_IRQ_DMA2_CHANNEL4  , .ChannelReg = LL_DMA_CHANNEL_4 , .ChannelIsrHandler = Dma_Dma2Channel4_IsrHandler },
    { .ChannelId = DMA_CHANNEL_5 , .ChannelIrq = NVIC_PERIPH_IRQ_DMA2_CHANNEL5  , .ChannelReg = LL_DMA_CHANNEL_5 , .ChannelIsrHandler = Dma_Dma2Channel5_IsrHandler },
    { .ChannelId = DMA_CHANNEL_6 , .ChannelIrq = NVIC_PERIPH_IRQ_DMA2_CHANNEL6  , .ChannelReg = LL_DMA_CHANNEL_6 , .ChannelIsrHandler = Dma_Dma2Channel6_IsrHandler },
    { .ChannelId = DMA_CHANNEL_7 , .ChannelIrq = NVIC_PERIPH_IRQ_DMA2_CHANNEL7  , .ChannelReg = LL_DMA_CHANNEL_7 , .ChannelIsrHandler = Dma_Dma2Channel7_IsrHandler },
};
#endif

static const volatile dma_PeriphConfig_t    dma_PeriphConf[ DMA_PERIPH_CNT ] =
{
    { .DmaPeriphId = DMA_PERIPH_1 , .DmaReg = DMA1 , .RccPeriphId = RCC_PERIPH_DMA1 , .ChannelsConfig = dma_Dma1IrqConfig , .IsrCallbacks = dma_Dma1IrqCallbacks },
#if defined(DMA2)
    { .DmaPeriphId = DMA_PERIPH_2 , .DmaReg = DMA2 , .RccPeriphId = RCC_PERIPH_DMA2 , .ChannelsConfig = dma_Dma2IrqConfig , .IsrCallbacks = dma_Dma2IrqCallbacks },
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
dma_RequestState_t Dma_Get_DefaultConfig( dma_ConfigStruct_t* dmaConfig )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;

    if( DMA_NULL_PTR != dmaConfig )
    {
        dmaConfig->DmaPeriphId         = DMA_PERIPH_1;
        dmaConfig->DmaChannel          = DMA_CHANNEL_1;
        dmaConfig->Direction           = DMA_DIR_PERIPH_TO_MEMORY;
        dmaConfig->TransferMode        = DMA_TRANSFER_MODE_NORMAL;
        dmaConfig->PeriphAddress       = 0x00;
        dmaConfig->MemoryAddress       = 0x00;
        dmaConfig->PeriphAddrIncrement = DMA_PERIPH_ADDR_STATIC;
        dmaConfig->MemoryAddrIncrement = DMA_MEMORY_ADDR_STATIC;
        dmaConfig->PeriphTransferSize  = DMA_PERIPH_TRANSFER_SIZE_BYTE;
        dmaConfig->MemoryTransferSize  = DMA_MEMORY_TRANSFER_SIZE_BYTE;
        dmaConfig->DataCount           = 0u;
        dmaConfig->PeripheralReqId     = DMA_REQ_MEM2MEM;
        dmaConfig->Priority            = 0u;

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
 * \param dmaConfig [in]: Configuration structure
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_StructInit( dma_ConfigStruct_t *dmaConfig )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;

    if( DMA_NULL_PTR != dmaConfig )
    {
        rcc_FunctionState_t rccActivationState = RCC_FUNCTION_INACTIVE;
        rcc_RequestState_t  rccRequestState    = RCC_REQUEST_ERROR;

        /*-------------- DMA peripheral clock activation section -------------*/
        rccRequestState = Rcc_Get_PeriphState( dma_PeriphConf[ dmaConfig->DmaPeriphId ].RccPeriphId, &rccActivationState );

        if( ( RCC_REQUEST_ERROR     != rccRequestState    ) &&
            ( RCC_FUNCTION_INACTIVE != rccActivationState )    )
        {
            rccRequestState = Rcc_Set_PeriphActive( dma_PeriphConf[ dmaConfig->DmaPeriphId ].RccPeriphId );

            if( RCC_REQUEST_ERROR == rccRequestState )
            {
                retState = DMA_REQUEST_ERROR;
                return ( retState );
            }
        }
        else
        {
            /* No clock activation needed */
        }

        /*------------- DMA Multiplexer clock activation section -------------*/
        rccRequestState = Rcc_Get_PeriphState( RCC_PERIPH_DMAMUX1, &rccActivationState );

        if( ( RCC_REQUEST_ERROR     != rccRequestState    ) &&
            ( RCC_FUNCTION_INACTIVE != rccActivationState )    )
        {
            rccRequestState = Rcc_Set_PeriphActive( RCC_PERIPH_DMAMUX1 );

            if( RCC_REQUEST_ERROR == rccRequestState )
            {
                retState = DMA_REQUEST_ERROR;
                return ( retState );
            }
        }
        else
        {
            /* No clock activation needed */
        }

        /*-------------------- DMA configuration section ---------------------*/
        retState = Dma_Set_PeripheralRequest( dmaConfig->DmaPeriphId, dmaConfig->DmaChannel, dmaConfig->PeripheralReqId );
        if( DMA_REQUEST_OK != retState )
        {
            return ( retState );
        }

        retState = Dma_Set_TransferMode( dmaConfig->DmaPeriphId, dmaConfig->DmaChannel, dmaConfig->TransferMode );
        if( DMA_REQUEST_OK != retState )
        {
            return ( retState );
        }

        retState = Dma_Set_Direction( dmaConfig->DmaPeriphId, dmaConfig->DmaChannel, dmaConfig->Direction );
        if( DMA_REQUEST_OK != retState )
        {
            return ( retState );
        }

        retState = Dma_Set_Priority( dmaConfig->DmaPeriphId, dmaConfig->DmaChannel, dmaConfig->Priority );
        if( DMA_REQUEST_OK != retState )
        {
            return ( retState );
        }

        retState = Dma_Set_PeriphAddrIncrement( dmaConfig->DmaPeriphId, dmaConfig->DmaChannel, dmaConfig->PeriphAddrIncrement );
        if( DMA_REQUEST_OK != retState )
        {
            return ( retState );
        }

        retState = Dma_Set_MemoryAddrIncrement( dmaConfig->DmaPeriphId, dmaConfig->DmaChannel, dmaConfig->MemoryAddrIncrement );
        if( DMA_REQUEST_OK != retState )
        {
            return ( retState );
        }

        retState = Dma_Set_PeriphTransferSize( dmaConfig->DmaPeriphId, dmaConfig->DmaChannel, dmaConfig->PeriphTransferSize );
        if( DMA_REQUEST_OK != retState )
        {
            return ( retState );
        }

        retState = Dma_Set_MemoryTransferSize( dmaConfig->DmaPeriphId, dmaConfig->DmaChannel, dmaConfig->MemoryTransferSize );
        if( DMA_REQUEST_OK != retState )
        {
            return ( retState );
        }

        retState = Dma_Set_PeriphAddr( dmaConfig->DmaPeriphId, dmaConfig->DmaChannel, dmaConfig->PeriphAddress );
        if( DMA_REQUEST_OK != retState )
        {
            return ( retState );
        }

        retState = Dma_Set_MemoryAddr( dmaConfig->DmaPeriphId, dmaConfig->DmaChannel, dmaConfig->MemoryAddress );
        if( DMA_REQUEST_OK != retState )
        {
            return ( retState );
        }

        if( DMA_NULL_PTR != dmaConfig->TransferCompleteCallback )
        {
            retState = Dma_Set_TransferCompleteIsrHandler( dmaConfig->DmaPeriphId, dmaConfig->DmaChannel, dmaConfig->TransferCompleteCallback );
            if( DMA_REQUEST_OK != retState )
            {
                return ( retState );
            }
        }

        if( DMA_NULL_PTR != dmaConfig->TransferCompleteCallback )
        {
            retState = Dma_Set_HalfTransferIsrHandler( dmaConfig->DmaPeriphId, dmaConfig->DmaChannel, dmaConfig->HalfTransferCallback );
            if( DMA_REQUEST_OK != retState )
            {
                return ( retState );
            }
        }

        if( DMA_NULL_PTR != dmaConfig->TransferCompleteCallback )
        {
            retState = Dma_Set_TransferErrorIsrHandler( dmaConfig->DmaPeriphId, dmaConfig->DmaChannel, dmaConfig->TransferErrorCallback );
            if( DMA_REQUEST_OK != retState )
            {
                return ( retState );
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
 * \brief Enables required channel for selected DMA peripheral
 *
 * \param dmaBus     [in]: DMA peripheral ID
 * \param dmaChannel [in]: DMA channel ID
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Set_ChannelActive( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;
    uint32_t           regValue = 0u;

    if( DMA_PERIPH_CNT > dmaBus )
    {
        LL_DMA_EnableChannel( dma_PeriphConf[ dmaBus ].DmaReg, dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg );

        for( uint32_t iterationCnt = 0u; DMA_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
        {
            regValue = LL_DMA_IsEnabledChannel( dma_PeriphConf[ dmaBus ].DmaReg, dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg );

            if( 0u != regValue )
            {
                retState = DMA_REQUEST_OK;
                break;
            }
            else
            {
                /* Clock source has not yet been changed, keep return state as error */
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
 * \brief Disables required channel for selected DMA peripheral
 *
 * \param dmaBus     [in]: DMA peripheral ID
 * \param dmaChannel [in]: DMA channel ID
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Set_ChannelInactive( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;
    uint32_t           regValue = 0u;

    if( DMA_PERIPH_CNT > dmaBus )
    {
        LL_DMA_DisableChannel( dma_PeriphConf[ dmaBus ].DmaReg, dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg );

        for( uint32_t iterationCnt = 0u; DMA_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
        {
            regValue = LL_DMA_IsEnabledChannel( dma_PeriphConf[ dmaBus ].DmaReg, dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg );

            if( 0u == regValue )
            {
                retState = DMA_REQUEST_OK;
                break;
            }
            else
            {
                /* Clock source has not yet been changed, keep return state as error */
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
 * \brief Reads activation status of required channel for selected DMA peripheral
 *
 * DMA channel can be activated/deactivated as user needs. This function provide
 * ability to read this activation status.
 *
 * \param dmaBus        [in]: DMA peripheral ID
 * \param dmaChannel    [in]: DMA channel ID
 * \param channelState [out]: Activation status
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Get_ChannelState( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_FunctionState_t *channelState )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;
    uint32_t           regValue = 0u;

    if( ( DMA_PERIPH_CNT > dmaBus       ) &&
        ( DMA_NULL_PTR  != channelState )    )
    {
        regValue = LL_DMA_IsEnabledChannel( dma_PeriphConf[ dmaBus ].DmaReg, dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg );

        if( 0u != regValue )
        {
            *channelState = DMA_FUNCTION_ACTIVE;
        }
        else
        {
            *channelState = DMA_FUNCTION_INACTIVE;
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
 * \brief Configures transfer direction of DMA channel.
 *
 * DMA channel can be configured in transferring multiple directions. It can
 * move data from peripheral to memory, from memory to peripheral and from
 * memory to memory.
 *
 * \param dmaBus     [in]: DMA peripheral ID
 * \param dmaChannel [in]: DMA channel ID
 * \param direction  [in]: Transfer direction configuration
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Set_Direction( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_Direction_t direction )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;
    uint32_t           regValue = 0u;

    if( DMA_PERIPH_CNT > dmaBus )
    {
        LL_DMA_SetDataTransferDirection( dma_PeriphConf[ dmaBus ].DmaReg,
                                         dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg,
                                         direction );

        for( uint32_t iterationCnt = 0u; DMA_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
        {
            regValue = LL_DMA_GetDataTransferDirection( dma_PeriphConf[ dmaBus ].DmaReg,
                                                        dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg );

            if( direction == regValue )
            {
                retState = DMA_REQUEST_OK;
                break;
            }
            else
            {
                /* Clock source has not yet been changed, keep return state as error */
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
 * \brief Returns transfer direction of DMA channel.
 *
 * \param dmaBus      [in]: DMA peripheral ID
 * \param dmaChannel  [in]: DMA channel ID
 * \param direction  [out]: Transfer direction configuration
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Get_Direction( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_Direction_t *direction )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;

    if( ( DMA_PERIPH_CNT > dmaBus    ) &&
        ( DMA_NULL_PTR  != direction )    )
    {
        *direction = LL_DMA_GetDataTransferDirection( dma_PeriphConf[ dmaBus ].DmaReg,
                                                      dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg );

        retState = DMA_REQUEST_OK;
    }
    else
    {
        retState = DMA_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Configures transfer mode of DMA channel.
 *
 * DMA channel can be configured in transferring multiple modes. It can
 * be stopped, after reaching target count of transfers, or it can start from
 * beginning of target location and fill target location again.
 *
 * \param dmaBus       [in]: DMA peripheral ID
 * \param dmaChannel   [in]: DMA channel ID
 * \param transferMode [in]: Transfer mode (normal or circular) configuration
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Set_TransferMode( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_TransferMode_t transferMode )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;
    uint32_t           regValue = 0u;

    if( DMA_PERIPH_CNT > dmaBus )
    {
        LL_DMA_SetMode( dma_PeriphConf[ dmaBus ].DmaReg,
                        dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg,
                        transferMode );

        for( uint32_t iterationCnt = 0u; DMA_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
        {
            regValue = LL_DMA_GetMode( dma_PeriphConf[ dmaBus ].DmaReg,
                                       dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg );

            if( transferMode == regValue )
            {
                retState = DMA_REQUEST_OK;
                break;
            }
            else
            {
                /* Clock source has not yet been changed, keep return state as error */
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
 * \brief Returns transfer mode of DMA channel.
 *
 * DMA channel can be configured in transferring multiple modes. It can
 * be stopped, after reaching target count of transfers, or it can start from
 * beginning of target location and fill target location again.
 *
 * \param dmaBus        [in]: DMA peripheral ID
 * \param dmaChannel    [in]: DMA channel ID
 * \param transferMode [out]: Transfer mode (normal or circular) configuration
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Get_TransferMode( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_TransferMode_t *transferMode )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;

    if( ( DMA_PERIPH_CNT > dmaBus       ) &&
        ( DMA_NULL_PTR  != transferMode )    )
    {
        *transferMode = LL_DMA_GetMode( dma_PeriphConf[ dmaBus ].DmaReg,
                                        dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg );

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
 * \param dmaChannel [in]: DMA channel ID
 * \param periphAddr [in]: Address of peripheral used for data transfer
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Set_PeriphAddr( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_PeriphAddr_t periphAddr )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;
    uint32_t           regValue = 0u;

    if( DMA_PERIPH_CNT > dmaBus )
    {
        LL_DMA_SetPeriphAddress( dma_PeriphConf[ dmaBus ].DmaReg,
                                 dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg,
                                 periphAddr );

        for( uint32_t iterationCnt = 0u; DMA_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
        {
            regValue = LL_DMA_GetPeriphAddress( dma_PeriphConf[ dmaBus ].DmaReg,
                                                dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg );

            if( periphAddr == regValue )
            {
                retState = DMA_REQUEST_OK;
                break;
            }
            else
            {
                /* Clock source has not yet been changed, keep return state as error */
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
 * \brief Returns DMA peripheral address.
 *
 * \param dmaBus      [in]: DMA peripheral ID
 * \param dmaChannel  [in]: DMA channel ID
 * \param periphAddr [out]: Address of peripheral used for data transfer
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Get_PeriphAddr( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_PeriphAddr_t *periphAddr )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;

    if( ( DMA_PERIPH_CNT > dmaBus     ) &&
        ( DMA_NULL_PTR  != periphAddr )    )
    {
        *periphAddr = LL_DMA_GetPeriphAddress( dma_PeriphConf[ dmaBus ].DmaReg,
                                               dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg );

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
 * \param dmaChannel [in]: DMA channel ID
 * \param periphAddr [in]: Memory address used for data transfer
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Set_MemoryAddr( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_MemoryAddr_t memoryAddr )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;
    uint32_t           regValue = 0u;

    if( DMA_PERIPH_CNT > dmaBus )
    {
        LL_DMA_SetMemoryAddress( dma_PeriphConf[ dmaBus ].DmaReg,
                                 dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg,
                                 memoryAddr );

        for( uint32_t iterationCnt = 0u; DMA_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
        {
            regValue = LL_DMA_GetMemoryAddress( dma_PeriphConf[ dmaBus ].DmaReg,
                                                dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg );

            if( memoryAddr == regValue )
            {
                retState = DMA_REQUEST_OK;
                break;
            }
            else
            {
                /* Clock source has not yet been changed, keep return state as error */
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
 * \brief Returns DMA memory address.
 *
 * \param dmaBus      [in]: DMA peripheral ID
 * \param dmaChannel  [in]: DMA channel ID
 * \param periphAddr [out]: Memory address used for data transfer
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Get_MemoryAddr( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_MemoryAddr_t *memoryAddr )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;

    if( ( DMA_PERIPH_CNT > dmaBus     ) &&
        ( DMA_NULL_PTR  != memoryAddr )    )
    {
        *memoryAddr = LL_DMA_GetPeriphAddress( dma_PeriphConf[ dmaBus ].DmaReg,
                                               dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg );

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
 * \param dmaChannel    [in]: DMA channel ID
 * \param periphAddrInc [in]: Peripheral address increment state
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Set_PeriphAddrIncrement( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_PeriphAddrInc_t periphAddrInc )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;
    uint32_t           regValue = 0u;

    if( DMA_PERIPH_CNT > dmaBus )
    {
        LL_DMA_SetPeriphIncMode( dma_PeriphConf[ dmaBus ].DmaReg,
                                 dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg,
                                 periphAddrInc );

        for( uint32_t iterationCnt = 0u; DMA_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
        {
            regValue = LL_DMA_GetPeriphIncMode( dma_PeriphConf[ dmaBus ].DmaReg,
                                                dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg );

            if( periphAddrInc == regValue )
            {
                retState = DMA_REQUEST_OK;
                break;
            }
            else
            {
                /* Clock source has not yet been changed, keep return state as error */
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
 * \brief Returns DMA peripheral address increment.
 *
 * \param dmaBus         [in]: DMA peripheral ID
 * \param dmaChannel     [in]: DMA channel ID
 * \param periphAddrInc [out]: Peripheral address increment state
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Get_PeriphAddrIncrement( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_PeriphAddrInc_t *periphAddrInc )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;

    if( ( DMA_PERIPH_CNT > dmaBus        ) &&
        ( DMA_NULL_PTR  != periphAddrInc )    )
    {
        *periphAddrInc = LL_DMA_GetPeriphIncMode( dma_PeriphConf[ dmaBus ].DmaReg,
                                                  dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg );

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
 * \param dmaChannel    [in]: DMA channel ID
 * \param memoryAddrInc [in]: Memory address increment state
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Set_MemoryAddrIncrement( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_MemoryAddrInc_t memoryAddrInc )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;
    uint32_t           regValue = 0u;

    if( DMA_PERIPH_CNT > dmaBus )
    {
        LL_DMA_SetMemoryIncMode( dma_PeriphConf[ dmaBus ].DmaReg,
                                 dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg,
                                 memoryAddrInc );

        for( uint32_t iterationCnt = 0u; DMA_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
        {
            regValue = LL_DMA_GetMemoryIncMode( dma_PeriphConf[ dmaBus ].DmaReg,
                                                dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg );

            if( memoryAddrInc == regValue )
            {
                retState = DMA_REQUEST_OK;
                break;
            }
            else
            {
                /* Clock source has not yet been changed, keep return state as error */
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
 * \brief Returns DMA memory address increment.
 *
 * \param dmaBus         [in]: DMA peripheral ID
 * \param dmaChannel     [in]: DMA channel ID
 * \param memoryAddrInc [out]: Memory address increment state
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Get_MemoryAddrIncrement( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_MemoryAddrInc_t *memoryAddrInc )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;

    if( ( DMA_PERIPH_CNT > dmaBus        ) &&
        ( DMA_NULL_PTR  != memoryAddrInc )    )
    {
        *memoryAddrInc = LL_DMA_GetMemoryIncMode( dma_PeriphConf[ dmaBus ].DmaReg,
                                                  dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg );

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
 * \param dmaChannel         [in]: DMA channel ID
 * \param periphTransferSize [in]: Peripheral transfer size.
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Set_PeriphTransferSize( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_PeriphTransferSize_t periphTransferSize)
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;
    uint32_t           regValue = 0u;

    if( DMA_PERIPH_CNT > dmaBus )
    {
        LL_DMA_SetPeriphSize( dma_PeriphConf[ dmaBus ].DmaReg,
                              dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg,
                              periphTransferSize );

        for( uint32_t iterationCnt = 0u; DMA_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
        {
            regValue = LL_DMA_GetPeriphSize( dma_PeriphConf[ dmaBus ].DmaReg,
                                             dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg );

            if( periphTransferSize == regValue )
            {
                retState = DMA_REQUEST_OK;
                break;
            }
            else
            {
                /* Clock source has not yet been changed, keep return state as error */
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
 * \brief Returns DMA peripheral transfer size.
 *
 * \param dmaBus              [in]: DMA peripheral ID
 * \param dmaChannel          [in]: DMA channel ID
 * \param periphTransferSize [out]: Peripheral transfer size
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Get_PeriphTransferSize( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_PeriphTransferSize_t *periphTransferSize )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;

    if( ( DMA_PERIPH_CNT > dmaBus             ) &&
        ( DMA_NULL_PTR  != periphTransferSize )    )
    {
        *periphTransferSize = LL_DMA_GetPeriphSize( dma_PeriphConf[ dmaBus ].DmaReg,
                                                    dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg );

        retState = DMA_REQUEST_OK;
    }
    else
    {
        retState = DMA_REQUEST_ERROR;
    }

    return ( retState );
}



/**
 * \brief Configures DMA transfer size.
 *
 * \param dmaBus             [in]: DMA peripheral ID
 * \param dmaChannel         [in]: DMA channel ID
 * \param memoryTransferSize [in]: Memory transfer size.
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Set_MemoryTransferSize( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_MemoryTransferSize_t memoryTransferSize )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;
    uint32_t           regValue = 0u;

    if( DMA_PERIPH_CNT > dmaBus )
    {
        LL_DMA_SetMemorySize( dma_PeriphConf[ dmaBus ].DmaReg,
                              dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg,
                              memoryTransferSize );

        for( uint32_t iterationCnt = 0u; DMA_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
        {
            regValue = LL_DMA_GetMemorySize( dma_PeriphConf[ dmaBus ].DmaReg,
                                             dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg );

            if( memoryTransferSize == regValue )
            {
                retState = DMA_REQUEST_OK;
                break;
            }
            else
            {
                /* Clock source has not yet been changed, keep return state as error */
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
 * \brief Returns DMA memory transfer size.
 *
 * \param dmaBus              [in]: DMA peripheral ID
 * \param dmaChannel          [in]: DMA channel ID
 * \param memoryTransferSize [out]: Memory transfer size.
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Get_MemoryTransferSize( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_MemoryTransferSize_t *memoryTransferSize )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;

    if( ( DMA_PERIPH_CNT > dmaBus             ) &&
        ( DMA_NULL_PTR  != memoryTransferSize )    )
    {
        *memoryTransferSize = LL_DMA_GetMemorySize( dma_PeriphConf[ dmaBus ].DmaReg,
                                                    dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg );

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
 * \param dmaChannel [in]: DMA channel ID
 * \param dataCount  [in]: Count of data to be transferred
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Set_DataCount( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_DataCount_t dataCount )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;
    uint32_t           regValue = 0u;

    if( DMA_PERIPH_CNT > dmaBus )
    {
        LL_DMA_SetDataLength( dma_PeriphConf[ dmaBus ].DmaReg,
                              dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg,
                              dataCount );

        for( uint32_t iterationCnt = 0u; DMA_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
        {
            regValue = LL_DMA_GetDataLength( dma_PeriphConf[ dmaBus ].DmaReg,
                                             dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg );

            if( dataCount == regValue )
            {
                retState = DMA_REQUEST_OK;
                break;
            }
            else
            {
                /* Clock source has not yet been changed, keep return state as error */
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
 * \brief Returns count of data to be transferred
 *
 * \param dmaBus     [in]: DMA peripheral ID
 * \param dmaChannel [in]: DMA channel ID
 * \param dataCount  [out]: Count of data to be transferred
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Get_DataCount( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_DataCount_t *dataCount )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;

    if( ( DMA_PERIPH_CNT > dmaBus    ) &&
        ( DMA_NULL_PTR  != dataCount )    )
    {
        *dataCount = LL_DMA_GetDataLength( dma_PeriphConf[ dmaBus ].DmaReg,
                                           dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg );

        retState = DMA_REQUEST_OK;
    }
    else
    {
        retState = DMA_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Configures DMA transfer trigger request ID
 *
 * \param dmaBus     [in]: DMA peripheral ID
 * \param dmaChannel [in]: DMA channel ID
 * \param requestId  [in]: Identification of peripheral triggering transfer request
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Set_PeripheralRequest( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_PeriphReqId_t requestId )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;
    uint32_t           regValue = 0u;

    if( DMA_PERIPH_CNT > dmaBus )
    {
        LL_DMA_SetPeriphRequest( dma_PeriphConf[ dmaBus ].DmaReg,
                                 dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg,
                                 requestId );

        for( uint32_t iterationCnt = 0u; DMA_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
        {
            regValue = LL_DMA_GetPeriphRequest( dma_PeriphConf[ dmaBus ].DmaReg,
                                                dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg );

            if( requestId == regValue )
            {
                retState = DMA_REQUEST_OK;
                break;
            }
            else
            {
                /* Clock source has not yet been changed, keep return state as error */
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
 * \brief Returns DMA transfer trigger request ID
 *
 * \param dmaBus      [in]: DMA peripheral ID
 * \param dmaChannel  [in]: DMA channel ID
 * \param requestId  [out]: Identification of peripheral triggering transfer request
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Get_PeripheralRequest( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_PeriphReqId_t *requestId )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;

    if( ( DMA_PERIPH_CNT > dmaBus    ) &&
        ( DMA_NULL_PTR  != requestId )    )
    {
        *requestId = LL_DMA_GetPeriphRequest( dma_PeriphConf[ dmaBus ].DmaReg,
                                              dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg );

        retState = DMA_REQUEST_OK;
    }
    else
    {
        retState = DMA_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Configures channel priority
 *
 * \param dmaBus     [in]: DMA peripheral ID
 * \param dmaChannel [in]: DMA channel ID
 * \param priority   [in]: Channel priority configuration
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Set_Priority( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_Priority_t priority )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;
    uint32_t           regValue = 0u;

    if( DMA_PERIPH_CNT > dmaBus )
    {
        LL_DMA_SetChannelPriorityLevel( dma_PeriphConf[ dmaBus ].DmaReg,
                                        dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg,
                                        priority );

        for( uint32_t iterationCnt = 0u; DMA_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
        {
            regValue = LL_DMA_GetChannelPriorityLevel( dma_PeriphConf[ dmaBus ].DmaReg,
                                                       dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg );

            if( priority == regValue )
            {
                retState = DMA_REQUEST_OK;
                break;
            }
            else
            {
                /* Clock source has not yet been changed, keep return state as error */
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
 * \brief Returns channel priority
 *
 * \param dmaBus     [in]: DMA peripheral ID
 * \param dmaChannel [in]: DMA channel ID
 * \param priority  [out]: Channel priority configuration
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Get_Priority( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_Priority_t *priority )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;

    if( ( DMA_PERIPH_CNT > dmaBus   ) &&
        ( DMA_NULL_PTR  != priority )    )
    {
        *priority = LL_DMA_GetChannelPriorityLevel( dma_PeriphConf[ dmaBus ].DmaReg,
                                                    dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg );

        retState = DMA_REQUEST_OK;
    }
    else
    {
        retState = DMA_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Activates transfer of selected DMA channel
 *
 * \param dmaBus         [in]: DMA peripheral ID
 * \param dmaChannel     [in]: DMA channel ID
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Set_TransferActive( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;
    uint32_t           regValue = 0u;

    if( ( DMA_PERIPH_CNT  > dmaBus     ) &&
        ( DMA_CHANNEL_CNT > dmaChannel )    )
    {
        LL_DMA_EnableChannel( dma_PeriphConf[ dmaBus ].DmaReg,
                              dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg );

        for( uint32_t iterationCnt = 0u; DMA_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
        {
            regValue = LL_DMA_IsEnabledChannel( dma_PeriphConf[ dmaBus ].DmaReg,
                                                dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg );

            if( 0u != regValue )
            {
                retState = DMA_REQUEST_OK;
                break;
            }
            else
            {
                /* Clock source has not yet been changed, keep return state as error */
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
 * \brief De-activates transfer of selected DMA channel
 *
 * \param dmaBus         [in]: DMA peripheral ID
 * \param dmaChannel     [in]: DMA channel ID
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Set_TransferInactive( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;
    uint32_t           regValue = 0u;

    if( ( DMA_PERIPH_CNT  > dmaBus     ) &&
        ( DMA_CHANNEL_CNT > dmaChannel )    )
    {
        LL_DMA_DisableChannel( dma_PeriphConf[ dmaBus ].DmaReg,
                              dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg );

        for( uint32_t iterationCnt = 0u; DMA_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
        {
            regValue = LL_DMA_IsEnabledChannel( dma_PeriphConf[ dmaBus ].DmaReg,
                                                dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg );

            if( 0u == regValue )
            {
                retState = DMA_REQUEST_OK;
                break;
            }
            else
            {
                /* Clock source has not yet been changed, keep return state as error */
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
 * \brief Returns transfer state of selected DMA channel
 *
 * \param dmaBus         [in]: DMA peripheral ID
 * \param dmaChannel     [in]: DMA channel ID
 * \param transferState [out]: Channel active/inactive state
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Get_TransferState( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_FunctionState_t *transferState )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;
    uint32_t           regValue = 0u;

    if( ( DMA_PERIPH_CNT > dmaBus        ) &&
        ( DMA_NULL_PTR  != transferState )    )
    {
        regValue = LL_DMA_IsEnabledChannel( dma_PeriphConf[ dmaBus ].DmaReg,
                                            dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg );

        if( 0u == regValue )
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

/*------------------------- Interrupts functionality -------------------------*/

/**
 * \brief Activates Interrupt Requests (IRQ) and configures Interrupt Service
 *        Routine (ISR) for selected channel.
 *
 * \param dmaBus     [in]: DMA peripheral ID
 * \param dmaChannel [in]: DMA channel ID
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Set_InterruptActive( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel )
{
    dma_RequestState_t  retState               = DMA_REQUEST_ERROR;
    nvic_RequestState_t nvicActivationState    = NVIC_REQUEST_ERROR;
    nvic_RequestState_t nvicHandlerConfigState = NVIC_REQUEST_ERROR;

    if( ( DMA_PERIPH_CNT  > dmaBus     ) &&
        ( DMA_CHANNEL_CNT > dmaChannel )    )
    {
        nvicHandlerConfigState = Nvic_Set_PeriphIrq_Handler( dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelIrq,
                                                             dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelIsrHandler );

        nvicActivationState = Nvic_Set_PeriphIrq_Active( dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelIrq );

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
 * \brief De-activates Interrupt Requests (IRQ) and configures Interrupt Service
 *        Routine (ISR) for selected channel.
 *
 * \param dmaBus     [in]: DMA peripheral ID
 * \param dmaChannel [in]: DMA channel ID
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Set_InterruptInactive( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel )
{
    dma_RequestState_t  retState  = DMA_REQUEST_ERROR;
    nvic_RequestState_t nvicState = NVIC_REQUEST_ERROR;

    if( ( DMA_PERIPH_CNT  > dmaBus     ) &&
        ( DMA_CHANNEL_CNT > dmaChannel )    )
    {
        nvicState = Nvic_Set_PeriphIrq_Inactive( dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelIrq );

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



dma_RequestState_t Dma_Set_TransferCompleteIrqActive( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;
    uint32_t           regValue = 0u;

    if( ( DMA_PERIPH_CNT  > dmaBus     ) &&
        ( DMA_CHANNEL_CNT > dmaChannel )    )
    {
        LL_DMA_EnableIT_TC( dma_PeriphConf[ dmaBus ].DmaReg,
                            dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg );

        for( uint32_t iterationCnt = 0u; DMA_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
        {
            regValue = LL_DMA_IsEnabledIT_TC( dma_PeriphConf[ dmaBus ].DmaReg,
                                              dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg );

            if( 0u != regValue )
            {
                retState = DMA_REQUEST_OK;
                break;
            }
            else
            {
                /* Clock source has not yet been changed, keep return state as error */
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


dma_RequestState_t Dma_Set_TransferCompleteIrqInactive( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;
    uint32_t           regValue = 0u;

    if( ( DMA_PERIPH_CNT  > dmaBus     ) &&
        ( DMA_CHANNEL_CNT > dmaChannel )    )
    {
        LL_DMA_DisableIT_TC( dma_PeriphConf[ dmaBus ].DmaReg,
                             dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg );

        for( uint32_t iterationCnt = 0u; DMA_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
        {
            regValue = LL_DMA_IsEnabledIT_TC( dma_PeriphConf[ dmaBus ].DmaReg,
                                              dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg );

            if( 0u == regValue )
            {
                retState = DMA_REQUEST_OK;
                break;
            }
            else
            {
                /* Clock source has not yet been changed, keep return state as error */
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

dma_RequestState_t Dma_Set_HalfTransferIrqActive( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;
    uint32_t           regValue = 0u;

    if( ( DMA_PERIPH_CNT  > dmaBus     ) &&
        ( DMA_CHANNEL_CNT > dmaChannel )    )
    {
        LL_DMA_EnableIT_HT( dma_PeriphConf[ dmaBus ].DmaReg,
                            dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg );

        for( uint32_t iterationCnt = 0u; DMA_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
        {
            regValue = LL_DMA_IsEnabledIT_HT( dma_PeriphConf[ dmaBus ].DmaReg,
                                              dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg );

            if( 0u != regValue )
            {
                retState = DMA_REQUEST_OK;
                break;
            }
            else
            {
                /* Clock source has not yet been changed, keep return state as error */
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


dma_RequestState_t Dma_Set_HalfTransferIrqInactive( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;
    uint32_t           regValue = 0u;

    if( ( DMA_PERIPH_CNT  > dmaBus     ) &&
        ( DMA_CHANNEL_CNT > dmaChannel )    )
    {
        LL_DMA_DisableIT_HT( dma_PeriphConf[ dmaBus ].DmaReg,
                             dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg );

        for( uint32_t iterationCnt = 0u; DMA_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
        {
            regValue = LL_DMA_IsEnabledIT_HT( dma_PeriphConf[ dmaBus ].DmaReg,
                                              dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg );

            if( 0u == regValue )
            {
                retState = DMA_REQUEST_OK;
                break;
            }
            else
            {
                /* Clock source has not yet been changed, keep return state as error */
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


dma_RequestState_t Dma_Set_TransferErrorIrqActive( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;
    uint32_t           regValue = 0u;

    if( ( DMA_PERIPH_CNT  > dmaBus     ) &&
        ( DMA_CHANNEL_CNT > dmaChannel )    )
    {
        LL_DMA_EnableIT_TE( dma_PeriphConf[ dmaBus ].DmaReg,
                            dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg );

        for( uint32_t iterationCnt = 0u; DMA_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
        {
            regValue = LL_DMA_IsEnabledIT_TE( dma_PeriphConf[ dmaBus ].DmaReg,
                                              dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg );

            if( 0u != regValue )
            {
                retState = DMA_REQUEST_OK;
                break;
            }
            else
            {
                /* Clock source has not yet been changed, keep return state as error */
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


dma_RequestState_t Dma_Set_TransferErrorIrqInactive( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;
    uint32_t           regValue = 0u;

    if( ( DMA_PERIPH_CNT  > dmaBus     ) &&
        ( DMA_CHANNEL_CNT > dmaChannel )    )
    {
        LL_DMA_DisableIT_TE( dma_PeriphConf[ dmaBus ].DmaReg,
                             dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg );

        for( uint32_t iterationCnt = 0u; DMA_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
        {
            regValue = LL_DMA_IsEnabledIT_TE( dma_PeriphConf[ dmaBus ].DmaReg,
                                              dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg );

            if( 0u == regValue )
            {
                retState = DMA_REQUEST_OK;
                break;
            }
            else
            {
                /* Clock source has not yet been changed, keep return state as error */
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


dma_RequestState_t Dma_Set_TransferCompleteIsrHandler( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_IsrCallback irqHandler )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;

    if( ( DMA_PERIPH_CNT  > dmaBus     ) &&
        ( DMA_CHANNEL_CNT > dmaChannel )    )
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


dma_RequestState_t Dma_Get_TransferCompleteIsrHandler( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_IsrCallback *irqHandler )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;

    if( ( DMA_PERIPH_CNT  > dmaBus     ) &&
        ( DMA_CHANNEL_CNT > dmaChannel ) &&
        ( DMA_NULL_PTR   != irqHandler )    )
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


dma_RequestState_t Dma_Set_HalfTransferIsrHandler( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_IsrCallback irqHandler )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;

    if( ( DMA_PERIPH_CNT  > dmaBus     ) &&
        ( DMA_CHANNEL_CNT > dmaChannel )    )
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


dma_RequestState_t Dma_Get_HalfTransferIsrHandler( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_IsrCallback *irqHandler )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;

    if( ( DMA_PERIPH_CNT  > dmaBus     ) &&
        ( DMA_CHANNEL_CNT > dmaChannel ) &&
        ( DMA_NULL_PTR   != irqHandler )    )
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


dma_RequestState_t Dma_Set_TransferErrorIsrHandler( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_IsrCallback irqHandler )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;

    if( ( DMA_PERIPH_CNT  > dmaBus     ) &&
        ( DMA_CHANNEL_CNT > dmaChannel )    )
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


dma_RequestState_t Dma_Get_TransferErrorIsrHandler( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_IsrCallback *irqHandler )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;

    if( ( DMA_PERIPH_CNT  > dmaBus     ) &&
        ( DMA_CHANNEL_CNT > dmaChannel ) &&
        ( DMA_NULL_PTR   != irqHandler )    )
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

/* =========================== INTERRUPT HANDLERS =========================== */

static void Dma_GlobalIsrHandler( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel )
{
    uint32_t flagStatus = 0u;
    flagStatus = DMA_GET_ACTIVE_IRQ_FLAG( dma_PeriphConf[ dmaBus ].DmaReg,
                                          dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg,
                                          DMA_TC_IRQ_FLAG );

    if( 0u != flagStatus )
    {
        DMA_CLEAR_ACTIVE_IRQ_FLAG( dma_PeriphConf[ dmaBus ].DmaReg,
                                   dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg,
                                   DMA_TC_IRQ_FLAG );

        if( DMA_NULL_PTR != dma_PeriphConf[ dmaBus ].IsrCallbacks[ dmaChannel ].TransferCompleteCallback )
        {
            dma_PeriphConf[ dmaBus ].IsrCallbacks[ dmaChannel ].TransferCompleteCallback();
        }
        else
        {
            /* Interrupt callback is not configured */
        }
    }

    flagStatus = DMA_GET_ACTIVE_IRQ_FLAG( dma_PeriphConf[ dmaBus ].DmaReg,
                                          dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg,
                                          DMA_HT_IRQ_FLAG );

    if( 0u != flagStatus )
    {
        DMA_CLEAR_ACTIVE_IRQ_FLAG( dma_PeriphConf[ dmaBus ].DmaReg,
                                   dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg,
                                   DMA_HT_IRQ_FLAG );

        if( DMA_NULL_PTR != dma_PeriphConf[ dmaBus ].IsrCallbacks[ dmaChannel ].HalfTransferCallback )
        {
            dma_PeriphConf[ dmaBus ].IsrCallbacks[ dmaChannel ].HalfTransferCallback();
        }
        else
        {
            /* Interrupt callback is not configured */
        }
    }

    flagStatus = DMA_GET_ACTIVE_IRQ_FLAG( dma_PeriphConf[ dmaBus ].DmaReg,
                                          dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg,
                                          DMA_TE_IRQ_FLAG );

    if( 0u != flagStatus )
    {
        DMA_CLEAR_ACTIVE_IRQ_FLAG( dma_PeriphConf[ dmaBus ].DmaReg,
                                   dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg,
                                   DMA_TE_IRQ_FLAG );

        if( DMA_NULL_PTR != dma_PeriphConf[ dmaBus ].IsrCallbacks[ dmaChannel ].TransferErrorCallback )
        {
            dma_PeriphConf[ dmaBus ].IsrCallbacks[ dmaChannel ].TransferErrorCallback();
        }
        else
        {
            /* Interrupt callback is not configured */
        }
    }
}

static void Dma_Dma1Channel1_IsrHandler( void )
{
    Dma_GlobalIsrHandler( DMA_PERIPH_1, DMA_CHANNEL_1 );
}


static void Dma_Dma1Channel2_IsrHandler( void )
{
    Dma_GlobalIsrHandler( DMA_PERIPH_1, DMA_CHANNEL_2 );
}


static void Dma_Dma1Channel3_IsrHandler( void )
{
    Dma_GlobalIsrHandler( DMA_PERIPH_1, DMA_CHANNEL_3 );
}


static void Dma_Dma1Channel4_IsrHandler( void )
{
    Dma_GlobalIsrHandler( DMA_PERIPH_1, DMA_CHANNEL_4 );
}


static void Dma_Dma1Channel5_IsrHandler( void )
{
    Dma_GlobalIsrHandler( DMA_PERIPH_1, DMA_CHANNEL_5 );
}


static void Dma_Dma1Channel6_IsrHandler( void )
{
    Dma_GlobalIsrHandler( DMA_PERIPH_1, DMA_CHANNEL_6 );
}


static void Dma_Dma1Channel7_IsrHandler( void )
{
    Dma_GlobalIsrHandler( DMA_PERIPH_1, DMA_CHANNEL_7 );
}


#if defined(DMA2)
static void Dma_Dma2Channel1_IsrHandler( void )
{
    Dma_GlobalIsrHandler( DMA_PERIPH_2, DMA_CHANNEL_1 );
}


static void Dma_Dma2Channel2_IsrHandler( void )
{
    Dma_GlobalIsrHandler( DMA_PERIPH_2, DMA_CHANNEL_2 );
}


static void Dma_Dma2Channel3_IsrHandler( void )
{
    Dma_GlobalIsrHandler( DMA_PERIPH_2, DMA_CHANNEL_3 );
}


static void Dma_Dma2Channel4_IsrHandler( void )
{
    Dma_GlobalIsrHandler( DMA_PERIPH_2, DMA_CHANNEL_4 );
}


static void Dma_Dma2Channel5_IsrHandler( void )
{
    Dma_GlobalIsrHandler( DMA_PERIPH_2, DMA_CHANNEL_5 );
}


static void Dma_Dma2Channel6_IsrHandler( void )
{
    Dma_GlobalIsrHandler( DMA_PERIPH_2, DMA_CHANNEL_6 );
}


static void Dma_Dma2Channel7_IsrHandler( void )
{
    Dma_GlobalIsrHandler( DMA_PERIPH_2, DMA_CHANNEL_7 );
}

#endif

/* ================================ TASKS =================================== */
