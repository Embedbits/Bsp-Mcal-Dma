/**
 * \author Mr.Nobody
 * \file Dma.c
 * \ingroup Dma
 * \brief Dma module common functionality
 *
 * STM32G4 Direct Memory Access (DMA) controllers DMA1 / DMA2 have 8 channels
 * (6 channels on STM32G411xB, G431, G441). Peripheral request of the channel
 * (\ref dma_PeriphReqId_t) is routed by request multiplexer DMAMUX1. The DMAMUX1
 * register map keeps 8 channel slots per DMA on every STM32G4: DMA1 channel n
 * uses slot n - 1, DMA2 channel n slot 8 + n - 1 (on the 6-channel devices slots
 * 6 / 7 of each DMA are not implemented - CMSIS DMAMUX1_Channel6 = DMAMUX1_BASE +
 * 0x20, bug AB#1172).
 *
 * \note Configuration registers of the channel are write protected by hardware
 *       while the channel is enabled. All configuration functions return error
 *       if the channel is enabled.
 *
 * \note Device errata ES0430 / ES0431 / ES0523 2.3.1 "DMA disable failure and
 *       error flag omission upon simultaneous transfer error and global flag
 *       clear": the global flag GIFx is cleared only while the channel is
 *       disabled, interrupt handler clears the specific flags (TCIFx, HTIFx,
 *       TEIFx) only.
 */
/* ============================== INCLUDES ================================== */
#include "Dma.h"                            /* Self include                   */
#include "Dma_Port.h"                       /* Own port file include          */
#include "Dma_Types.h"                      /* Module types definitions       */
#include "Stm32_dma.h"                      /* DMA Registers definitions      */
#include "Rcc_Port.h"                       /* RCC module functionality       */
#include "Nvic_Port.h"                      /* NVIC module functionality      */
#include "Stm32_dmamux.h"                   /* DMAMUX registers definitions   */
/* ========================== SYMBOLIC CONSTANTS ============================ */

/** Value of major version of SW module */
#define DMA_MAJOR_VERSION           ( 1u )

/** Value of minor version of SW module */
#define DMA_MINOR_VERSION           ( 0u )

/** Value of patch version of SW module */
#define DMA_PATCH_VERSION           ( 0u )

/** Maximum timeout used for waiting for register update verification */
#define DMA_TIMEOUT_RAW             ( 0x84FCB )

/** Distance of flags of consecutive channels in ISR / IFCR registers */
#define DMA_FLAG_CHANNEL_SHIFT      ( DMA_ISR_GIF2_Pos - DMA_ISR_GIF1_Pos )

/** Global flag of the channel (relative to channel 1 position in ISR / IFCR) */
#define DMA_FLAG_GI                 ( DMA_ISR_GIF1 )

/** Transfer error flag of the channel (relative to channel 1 position in ISR / IFCR) */
#define DMA_FLAG_TE                 ( DMA_ISR_TEIF1 )

/** Half transfer flag of the channel (relative to channel 1 position in ISR / IFCR) */
#define DMA_FLAG_HT                 ( DMA_ISR_HTIF1 )

/** Transfer complete flag of the channel (relative to channel 1 position in ISR / IFCR) */
#define DMA_FLAG_TC                 ( DMA_ISR_TCIF1 )

/** All flags of the channel - cleared only while the channel is disabled (device
 *  errata 2.3.1: GIFx must not be cleared while the channel is active) */
#define DMA_FLAG_ALL                ( DMA_FLAG_GI | DMA_FLAG_TE | DMA_FLAG_HT | DMA_FLAG_TC )

/** Interrupt enable bits of the channel in CCR register */
#define DMA_CHANNEL_IT_ALL          ( DMA_CCR_TCIE | DMA_CCR_HTIE | DMA_CCR_TEIE )

/** Maximal DMAMUX1 request identification (DMAREQ_ID field) */
#define DMA_REQ_ID_MAX              ( DMAMUX_CxCR_DMAREQ_ID )

/** Distance of registers of consecutive DMAMUX1 channels */
#define DMA_MUX_CHANNEL_SIZE        ( DMAMUX1_Channel1_BASE - DMAMUX1_Channel0_BASE )

/** DMAMUX1 channel register slots per DMA peripheral - 8 on every STM32G4, also on the devices
 *  with 6 channels per DMA (slots 6 / 7 not implemented, bug AB#1172) */
#define DMA_MUX_SLOTS_PER_DMA       ( 8u )

/* =============================== MACROS =================================== */

/** Channel registers of selected DMA peripheral and channel */
#define DMA_CHANNEL_REG( dmaBus, dmaChannel )   ( (DMA_Channel_TypeDef *)( (uintptr_t)dma_PeriphConf[ dmaBus ].DmaReg + \
                                                  dma_ChannelRegOffsetLut[ dmaChannel ] ) )

/** DMAMUX1 channel registers of selected DMA peripheral and channel */
#define DMA_MUX_CHANNEL_REG( dmaBus, dmaChannel ) ( (DMAMUX_Channel_TypeDef *)( DMAMUX1_Channel0_BASE + \
                                                  ( ( ( (uint32_t)( dmaBus ) * DMA_MUX_SLOTS_PER_DMA ) + (uint32_t)( dmaChannel ) ) * \
                                                    DMA_MUX_CHANNEL_SIZE ) ) )

/* DMAMUX1 register of DMA2 channel 1 follows the 8 slots of DMA1 (CMSIS register map) */
#if defined(DMA1_Channel8)
_Static_assert( DMAMUX1_Channel8_BASE == ( DMAMUX1_Channel0_BASE + ( DMA_MUX_SLOTS_PER_DMA * DMA_MUX_CHANNEL_SIZE ) ), "Dma: DMAMUX1 slot of DMA2 channel 1 does not match CMSIS." );
#else
_Static_assert( DMAMUX1_Channel6_BASE == ( DMAMUX1_Channel0_BASE + ( DMA_MUX_SLOTS_PER_DMA * DMA_MUX_CHANNEL_SIZE ) ), "Dma: DMAMUX1 slot of DMA2 channel 1 does not match CMSIS." );
#endif /* DMA1_Channel8 */

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
    dma_ChannelId_t      ChannelId;         /**< Channel identification                                */
    nvic_PeriphIrqList_t ChannelIrq;        /**< Channel interrupt identification used by NVIC module  */
    uint32_t             ChannelReg;        /**< Channel identification used for register operations   */
    dma_IsrHandler_t     ChannelIsrHandler; /**< Channel Interrupt Status Routine (ISR) handler        */
}   dma_ChannelsConfig_t;

/** \brief Structure type used by DMA configuration array */
typedef struct
{
          dma_PeriphId_t        DmaPeriphId;          /**< DMA bus identification    */
          rcc_PeriphId_t        RccPeriphId;          /**< RCC bus identification    */
          DMA_TypeDef          *DmaReg;               /**< DMA bus register address  */
    const dma_ChannelsConfig_t * const ChannelsConfig; /**< DMA channels configuration */
    volatile dma_IsrCallbacks_t* const IsrCallbacks;  /**< ISR callback's array      */
}   dma_PeriphConfig_t;

/* ======================== FORWARD DECLARATIONS ============================ */

static uint32_t Dma_Get_Flags                ( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel );
static void     Dma_Clear_Flags              ( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, uint32_t flags );
static dma_RequestState_t Dma_Set_ClockActive( rcc_PeriphId_t rccPeriphId );

static void Dma_GlobalIsrHandler( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel );

static void Dma_Dma1Channel1_IsrHandler( void );
static void Dma_Dma1Channel2_IsrHandler( void );
static void Dma_Dma1Channel3_IsrHandler( void );
static void Dma_Dma1Channel4_IsrHandler( void );
static void Dma_Dma1Channel5_IsrHandler( void );
static void Dma_Dma1Channel6_IsrHandler( void );
#if defined(DMA1_Channel7)
static void Dma_Dma1Channel7_IsrHandler( void );
#endif
#if defined(DMA1_Channel8)
static void Dma_Dma1Channel8_IsrHandler( void );
#endif
#if defined(DMA2)
static void Dma_Dma2Channel1_IsrHandler( void );
static void Dma_Dma2Channel2_IsrHandler( void );
static void Dma_Dma2Channel3_IsrHandler( void );
static void Dma_Dma2Channel4_IsrHandler( void );
static void Dma_Dma2Channel5_IsrHandler( void );
static void Dma_Dma2Channel6_IsrHandler( void );
#if defined(DMA1_Channel7)
static void Dma_Dma2Channel7_IsrHandler( void );
#endif
#if defined(DMA1_Channel8)
static void Dma_Dma2Channel8_IsrHandler( void );
#endif
#endif

/* ========================== EXPORTED VARIABLES ============================ */

/* =========================== LOCAL VARIABLES ============================== */

/** Offset of channel registers from DMA peripheral base address */
static const uint8_t                        dma_ChannelRegOffsetLut[ DMA_CHANNEL_CNT ] =
{
    (uint8_t)( DMA1_Channel1_BASE - DMA1_BASE ),
    (uint8_t)( DMA1_Channel2_BASE - DMA1_BASE ),
    (uint8_t)( DMA1_Channel3_BASE - DMA1_BASE ),
    (uint8_t)( DMA1_Channel4_BASE - DMA1_BASE ),
    (uint8_t)( DMA1_Channel5_BASE - DMA1_BASE ),
    (uint8_t)( DMA1_Channel6_BASE - DMA1_BASE ),
#if defined(DMA1_Channel7)
    (uint8_t)( DMA1_Channel7_BASE - DMA1_BASE ),
#endif
#if defined(DMA1_Channel8)
    (uint8_t)( DMA1_Channel8_BASE - DMA1_BASE ),
#endif
};

/** Interrupt callbacks of DMA1 channels (zero initialized - no callback) */
static volatile dma_IsrCallbacks_t          dma_Dma1IrqCallbacks[ DMA_CHANNEL_CNT ];

#if defined(DMA2)
/** Interrupt callbacks of DMA2 channels (zero initialized - no callback) */
static volatile dma_IsrCallbacks_t          dma_Dma2IrqCallbacks[ DMA_CHANNEL_CNT ];
#endif

static const dma_ChannelsConfig_t           dma_Dma1IrqConfig[ DMA_CHANNEL_CNT ] =
{
    { .ChannelId = DMA_CHANNEL_1 , .ChannelIrq = NVIC_PERIPH_IRQ_DMA1_CHANNEL1 , .ChannelReg = LL_DMA_CHANNEL_1 , .ChannelIsrHandler = Dma_Dma1Channel1_IsrHandler },
    { .ChannelId = DMA_CHANNEL_2 , .ChannelIrq = NVIC_PERIPH_IRQ_DMA1_CHANNEL2 , .ChannelReg = LL_DMA_CHANNEL_2 , .ChannelIsrHandler = Dma_Dma1Channel2_IsrHandler },
    { .ChannelId = DMA_CHANNEL_3 , .ChannelIrq = NVIC_PERIPH_IRQ_DMA1_CHANNEL3 , .ChannelReg = LL_DMA_CHANNEL_3 , .ChannelIsrHandler = Dma_Dma1Channel3_IsrHandler },
    { .ChannelId = DMA_CHANNEL_4 , .ChannelIrq = NVIC_PERIPH_IRQ_DMA1_CHANNEL4 , .ChannelReg = LL_DMA_CHANNEL_4 , .ChannelIsrHandler = Dma_Dma1Channel4_IsrHandler },
    { .ChannelId = DMA_CHANNEL_5 , .ChannelIrq = NVIC_PERIPH_IRQ_DMA1_CHANNEL5 , .ChannelReg = LL_DMA_CHANNEL_5 , .ChannelIsrHandler = Dma_Dma1Channel5_IsrHandler },
    { .ChannelId = DMA_CHANNEL_6 , .ChannelIrq = NVIC_PERIPH_IRQ_DMA1_CHANNEL6 , .ChannelReg = LL_DMA_CHANNEL_6 , .ChannelIsrHandler = Dma_Dma1Channel6_IsrHandler },
#if defined(DMA1_Channel7)
    { .ChannelId = DMA_CHANNEL_7 , .ChannelIrq = NVIC_PERIPH_IRQ_DMA1_CHANNEL7 , .ChannelReg = LL_DMA_CHANNEL_7 , .ChannelIsrHandler = Dma_Dma1Channel7_IsrHandler },
#endif
#if defined(DMA1_Channel8)
    { .ChannelId = DMA_CHANNEL_8 , .ChannelIrq = NVIC_PERIPH_IRQ_DMA1_CHANNEL8 , .ChannelReg = LL_DMA_CHANNEL_8 , .ChannelIsrHandler = Dma_Dma1Channel8_IsrHandler },
#endif
};

#if defined(DMA2)
static const dma_ChannelsConfig_t           dma_Dma2IrqConfig[ DMA_CHANNEL_CNT ] =
{
    { .ChannelId = DMA_CHANNEL_1 , .ChannelIrq = NVIC_PERIPH_IRQ_DMA2_CHANNEL1 , .ChannelReg = LL_DMA_CHANNEL_1 , .ChannelIsrHandler = Dma_Dma2Channel1_IsrHandler },
    { .ChannelId = DMA_CHANNEL_2 , .ChannelIrq = NVIC_PERIPH_IRQ_DMA2_CHANNEL2 , .ChannelReg = LL_DMA_CHANNEL_2 , .ChannelIsrHandler = Dma_Dma2Channel2_IsrHandler },
    { .ChannelId = DMA_CHANNEL_3 , .ChannelIrq = NVIC_PERIPH_IRQ_DMA2_CHANNEL3 , .ChannelReg = LL_DMA_CHANNEL_3 , .ChannelIsrHandler = Dma_Dma2Channel3_IsrHandler },
    { .ChannelId = DMA_CHANNEL_4 , .ChannelIrq = NVIC_PERIPH_IRQ_DMA2_CHANNEL4 , .ChannelReg = LL_DMA_CHANNEL_4 , .ChannelIsrHandler = Dma_Dma2Channel4_IsrHandler },
    { .ChannelId = DMA_CHANNEL_5 , .ChannelIrq = NVIC_PERIPH_IRQ_DMA2_CHANNEL5 , .ChannelReg = LL_DMA_CHANNEL_5 , .ChannelIsrHandler = Dma_Dma2Channel5_IsrHandler },
    { .ChannelId = DMA_CHANNEL_6 , .ChannelIrq = NVIC_PERIPH_IRQ_DMA2_CHANNEL6 , .ChannelReg = LL_DMA_CHANNEL_6 , .ChannelIsrHandler = Dma_Dma2Channel6_IsrHandler },
#if defined(DMA1_Channel7)
    { .ChannelId = DMA_CHANNEL_7 , .ChannelIrq = NVIC_PERIPH_IRQ_DMA2_CHANNEL7 , .ChannelReg = LL_DMA_CHANNEL_7 , .ChannelIsrHandler = Dma_Dma2Channel7_IsrHandler },
#endif
#if defined(DMA1_Channel8)
    { .ChannelId = DMA_CHANNEL_8 , .ChannelIrq = NVIC_PERIPH_IRQ_DMA2_CHANNEL8 , .ChannelReg = LL_DMA_CHANNEL_8 , .ChannelIsrHandler = Dma_Dma2Channel8_IsrHandler },
#endif
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
dma_RequestState_t Dma_Get_DefaultConfig( dma_ConfigStruct_t * const dmaConfig )
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
 * Activates DMA peripheral and DMAMUX1 clock, disables the channel
 * (configuration registers are write protected while the channel is enabled)
 * and configures it. Interrupt callbacks of the channel are replaced by
 * callbacks of the configuration (null pointer clears the callback).
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
        ( DMA_CHANNEL_CNT  > dmaConfig->DmaChannel  )    )
    {
        /*------------ DMA peripheral and DMAMUX1 clock activation -----------*/
        retState = Dma_Set_ClockActive( dma_PeriphConf[ dmaConfig->DmaPeriphId ].RccPeriphId );
        if( DMA_REQUEST_OK != retState )
        {
            return ( retState );
        }

        retState = Dma_Set_ClockActive( RCC_PERIPH_DMAMUX1 );
        if( DMA_REQUEST_OK != retState )
        {
            return ( retState );
        }

        /*-------------------- DMA configuration section ---------------------*/
        retState = Dma_Set_TransferInactive( dmaConfig->DmaPeriphId, dmaConfig->DmaChannel );
        if( DMA_REQUEST_OK != retState )
        {
            return ( retState );
        }

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

        retState = Dma_Set_DataCount( dmaConfig->DmaPeriphId, dmaConfig->DmaChannel, dmaConfig->DataCount );
        if( DMA_REQUEST_OK != retState )
        {
            return ( retState );
        }

        retState = Dma_Set_TransferCompleteIsrHandler( dmaConfig->DmaPeriphId, dmaConfig->DmaChannel, dmaConfig->TransferCompleteCallback );
        if( DMA_REQUEST_OK != retState )
        {
            return ( retState );
        }

        retState = Dma_Set_HalfTransferIsrHandler( dmaConfig->DmaPeriphId, dmaConfig->DmaChannel, dmaConfig->HalfTransferCallback );
        if( DMA_REQUEST_OK != retState )
        {
            return ( retState );
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
 * \brief Activates transfer of selected DMA channel
 *
 * All flags of the channel are cleared before the channel is enabled. Request
 * succeeds if the channel is enabled or the transfer has already completed
 * (short memory to memory transfer). Transfer error detected after the
 * channel enable is reported as error.
 *
 * \note Memory to memory direction cannot be used with the circular mode
 *       (RM0440) - such channel configuration is refused, the channel is not
 *       enabled.
 *
 * \param dmaBus         [in]: DMA peripheral ID
 * \param dmaChannel     [in]: DMA channel ID
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Set_TransferActive( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel )
{
    dma_RequestState_t retState  = DMA_REQUEST_ERROR;
    uint32_t           regValue  = 0u;
    uint32_t           flagValue = 0u;

    if( ( DMA_PERIPH_CNT  > dmaBus     ) &&
        ( DMA_CHANNEL_CNT > dmaChannel )    )
    {
        const uint32_t ccrValue = READ_REG( DMA_CHANNEL_REG( dmaBus, dmaChannel )->CCR );

        if( ( DMA_CCR_MEM2MEM == ( ccrValue & DMA_CCR_MEM2MEM ) ) &&
            ( DMA_CCR_CIRC    == ( ccrValue & DMA_CCR_CIRC    ) )    )
        {
            /* Circular mode is not allowed in memory to memory direction */
            retState = DMA_REQUEST_ERROR;
        }
        else
        {
            retState = DMA_REQUEST_OK;
        }
    }
    else
    {
        retState = DMA_REQUEST_ERROR;
    }

    if( DMA_REQUEST_OK == retState )
    {
        Dma_Clear_Flags( dmaBus, dmaChannel, DMA_FLAG_ALL );

        LL_DMA_EnableChannel( dma_PeriphConf[ dmaBus ].DmaReg,
                             dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg );

        for( uint32_t iterationCnt = 0u; DMA_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
        {
            regValue  = LL_DMA_IsEnabledChannel( dma_PeriphConf[ dmaBus ].DmaReg,
                                                dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg );
            flagValue = Dma_Get_Flags( dmaBus, dmaChannel );

            if( 0u != ( DMA_FLAG_TE & flagValue ) )
            {
                /* Channel was disabled by hardware due to transfer error */
                retState = DMA_REQUEST_ERROR;
                break;
            }
            else if( ( 0u != regValue                     ) ||
                     ( 0u != ( DMA_FLAG_TC & flagValue )  ) ||
                     ( 0u == LL_DMA_GetDataLength( dma_PeriphConf[ dmaBus ].DmaReg,
                                                   dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg ) ) )
            {
                /* Channel is running or the transfer has already completed */
                retState = DMA_REQUEST_OK;
                break;
            }
            else
            {
                /* Channel has not yet been enabled, keep return state as error */
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
 * Interrupts of the channel are masked while the channel is disabled, flags of
 * the aborted transfer are cleared after the channel is disabled. Callbacks are
 * not executed for aborted transfer.
 *
 * \param dmaBus         [in]: DMA peripheral ID
 * \param dmaChannel     [in]: DMA channel ID
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Set_TransferInactive( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel )
{
    dma_RequestState_t  retState  = DMA_REQUEST_ERROR;
    uint32_t            regValue  = 0u;
    uint32_t            itEnabled = 0u;
    DMA_Channel_TypeDef *channelReg = DMA_NULL_PTR;

    if( ( DMA_PERIPH_CNT  > dmaBus     ) &&
        ( DMA_CHANNEL_CNT > dmaChannel )    )
    {
        channelReg = DMA_CHANNEL_REG( dmaBus, dmaChannel );
        itEnabled  = READ_BIT( channelReg->CCR, DMA_CHANNEL_IT_ALL );

        CLEAR_BIT( channelReg->CCR, DMA_CHANNEL_IT_ALL );

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
            /* Channel is still enabled, flags of the transfer are kept */
        }

        SET_BIT( channelReg->CCR, itEnabled );
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
dma_RequestState_t Dma_Get_TransferState( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_FunctionState_t * const transferState )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;
    uint32_t           regValue = 0u;

    if( ( DMA_PERIPH_CNT   > dmaBus        ) &&
        ( DMA_CHANNEL_CNT  > dmaChannel    ) &&
        ( DMA_NULL_PTR    != transferState )    )
    {
        regValue = LL_DMA_IsEnabledChannel( dma_PeriphConf[ dmaBus ].DmaReg,
                                           dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg );

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
 * \brief Enables required channel for selected DMA peripheral
 *
 * Equivalent of \ref Dma_Set_TransferActive (channel enable starts transfer).
 *
 * \param dmaBus     [in]: DMA peripheral ID
 * \param dmaChannel [in]: DMA channel ID
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Set_ChannelActive( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel )
{
    return ( Dma_Set_TransferActive( dmaBus, dmaChannel ) );
}


/**
 * \brief Disables required channel for selected DMA peripheral
 *
 * Equivalent of \ref Dma_Set_TransferInactive.
 *
 * \param dmaBus     [in]: DMA peripheral ID
 * \param dmaChannel [in]: DMA channel ID
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Set_ChannelInactive( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel )
{
    return ( Dma_Set_TransferInactive( dmaBus, dmaChannel ) );
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
dma_RequestState_t Dma_Get_ChannelState( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_FunctionState_t * const channelState )
{
    return ( Dma_Get_TransferState( dmaBus, dmaChannel, channelState ) );
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

    if( ( DMA_PERIPH_CNT  > dmaBus     ) &&
        ( DMA_CHANNEL_CNT > dmaChannel ) &&
        ( ( DMA_DIR_PERIPH_TO_MEMORY == direction ) ||
          ( DMA_DIR_MEMORY_TO_PERIPH == direction ) ||
          ( DMA_DIR_MEMORY_TO_MEMORY == direction )    ) )
    {
        if( 0u != LL_DMA_IsEnabledChannel( dma_PeriphConf[ dmaBus ].DmaReg,
                                          dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg ) )
        {
            /* Configuration is write protected while the channel is enabled */
            retState = DMA_REQUEST_ERROR;
        }
        else
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
 * \brief Returns transfer direction of DMA channel.
 *
 * \param dmaBus      [in]: DMA peripheral ID
 * \param dmaChannel  [in]: DMA channel ID
 * \param direction  [out]: Transfer direction configuration
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Get_Direction( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_Direction_t * const direction )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;

    if( ( DMA_PERIPH_CNT   > dmaBus     ) &&
        ( DMA_CHANNEL_CNT  > dmaChannel ) &&
        ( DMA_NULL_PTR    != direction  )    )
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

    if( ( DMA_PERIPH_CNT  > dmaBus     ) &&
        ( DMA_CHANNEL_CNT > dmaChannel ) &&
        ( ( DMA_TRANSFER_MODE_NORMAL   == transferMode ) ||
          ( DMA_TRANSFER_MODE_CIRCULAR == transferMode )    ) )
    {
        if( 0u != LL_DMA_IsEnabledChannel( dma_PeriphConf[ dmaBus ].DmaReg,
                                          dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg ) )
        {
            /* Configuration is write protected while the channel is enabled */
            retState = DMA_REQUEST_ERROR;
        }
        else
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
dma_RequestState_t Dma_Get_TransferMode( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_TransferMode_t * const transferMode )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;

    if( ( DMA_PERIPH_CNT   > dmaBus       ) &&
        ( DMA_CHANNEL_CNT  > dmaChannel   ) &&
        ( DMA_NULL_PTR    != transferMode )    )
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
 *                         (source address in memory to memory direction)
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Set_PeriphAddr( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_PeriphAddr_t periphAddr )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;
    uint32_t           regValue = 0u;

    if( ( DMA_PERIPH_CNT  > dmaBus     ) &&
        ( DMA_CHANNEL_CNT > dmaChannel )    )
    {
        if( 0u != LL_DMA_IsEnabledChannel( dma_PeriphConf[ dmaBus ].DmaReg,
                                          dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg ) )
        {
            /* Configuration is write protected while the channel is enabled */
            retState = DMA_REQUEST_ERROR;
        }
        else
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
 * \param dmaChannel  [in]: DMA channel ID
 * \param periphAddr [out]: Address of peripheral used for data transfer
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Get_PeriphAddr( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_PeriphAddr_t * const periphAddr )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;

    if( ( DMA_PERIPH_CNT   > dmaBus     ) &&
        ( DMA_CHANNEL_CNT  > dmaChannel ) &&
        ( DMA_NULL_PTR    != periphAddr )    )
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
 * \param memoryAddr [in]: Memory address used for data transfer
 *                         (destination address in memory to memory direction)
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Set_MemoryAddr( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_MemoryAddr_t memoryAddr )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;
    uint32_t           regValue = 0u;

    if( ( DMA_PERIPH_CNT  > dmaBus     ) &&
        ( DMA_CHANNEL_CNT > dmaChannel )    )
    {
        if( 0u != LL_DMA_IsEnabledChannel( dma_PeriphConf[ dmaBus ].DmaReg,
                                          dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg ) )
        {
            /* Configuration is write protected while the channel is enabled */
            retState = DMA_REQUEST_ERROR;
        }
        else
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
 * \param dmaChannel  [in]: DMA channel ID
 * \param memoryAddr [out]: Memory address used for data transfer
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Get_MemoryAddr( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_MemoryAddr_t * const memoryAddr )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;

    if( ( DMA_PERIPH_CNT   > dmaBus     ) &&
        ( DMA_CHANNEL_CNT  > dmaChannel ) &&
        ( DMA_NULL_PTR    != memoryAddr )    )
    {
        *memoryAddr = LL_DMA_GetMemoryAddress( dma_PeriphConf[ dmaBus ].DmaReg,
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

    if( ( DMA_PERIPH_CNT  > dmaBus     ) &&
        ( DMA_CHANNEL_CNT > dmaChannel ) &&
        ( ( DMA_PERIPH_ADDR_INCREMENT == periphAddrInc ) ||
          ( DMA_PERIPH_ADDR_STATIC    == periphAddrInc )    ) )
    {
        if( 0u != LL_DMA_IsEnabledChannel( dma_PeriphConf[ dmaBus ].DmaReg,
                                          dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg ) )
        {
            /* Configuration is write protected while the channel is enabled */
            retState = DMA_REQUEST_ERROR;
        }
        else
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
 * \param dmaChannel     [in]: DMA channel ID
 * \param periphAddrInc [out]: Peripheral address increment state
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Get_PeriphAddrIncrement( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_PeriphAddrInc_t * const periphAddrInc )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;

    if( ( DMA_PERIPH_CNT   > dmaBus        ) &&
        ( DMA_CHANNEL_CNT  > dmaChannel    ) &&
        ( DMA_NULL_PTR    != periphAddrInc )    )
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

    if( ( DMA_PERIPH_CNT  > dmaBus     ) &&
        ( DMA_CHANNEL_CNT > dmaChannel ) &&
        ( ( DMA_MEMORY_ADDR_INCREMENT == memoryAddrInc ) ||
          ( DMA_MEMORY_ADDR_STATIC    == memoryAddrInc )    ) )
    {
        if( 0u != LL_DMA_IsEnabledChannel( dma_PeriphConf[ dmaBus ].DmaReg,
                                          dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg ) )
        {
            /* Configuration is write protected while the channel is enabled */
            retState = DMA_REQUEST_ERROR;
        }
        else
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
 * \param dmaChannel     [in]: DMA channel ID
 * \param memoryAddrInc [out]: Memory address increment state
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Get_MemoryAddrIncrement( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_MemoryAddrInc_t * const memoryAddrInc )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;

    if( ( DMA_PERIPH_CNT   > dmaBus        ) &&
        ( DMA_CHANNEL_CNT  > dmaChannel    ) &&
        ( DMA_NULL_PTR    != memoryAddrInc )    )
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
dma_RequestState_t Dma_Set_PeriphTransferSize( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_TransferSize_t periphTransferSize )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;
    uint32_t           regValue = 0u;
    uint32_t           reqValue = 0u;

    if( ( DMA_PERIPH_CNT        > dmaBus             ) &&
        ( DMA_CHANNEL_CNT       > dmaChannel         ) &&
        ( DMA_TRANSFER_SIZE_CNT > periphTransferSize )    )
    {
        if( 0u != LL_DMA_IsEnabledChannel( dma_PeriphConf[ dmaBus ].DmaReg,
                                          dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg ) )
        {
            /* Configuration is write protected while the channel is enabled */
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
                                  dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg,
                                  reqValue );

            for( uint32_t iterationCnt = 0u; DMA_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
            {
                regValue = LL_DMA_GetPeriphSize( dma_PeriphConf[ dmaBus ].DmaReg,
                                                 dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg );

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
 * \param dmaChannel          [in]: DMA channel ID
 * \param periphTransferSize [out]: Peripheral transfer size
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Get_PeriphTransferSize( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_TransferSize_t * const periphTransferSize )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;
    uint32_t           regValue = 0u;

    if( ( DMA_PERIPH_CNT   > dmaBus             ) &&
        ( DMA_CHANNEL_CNT  > dmaChannel         ) &&
        ( DMA_NULL_PTR    != periphTransferSize )    )
    {
        regValue = LL_DMA_GetPeriphSize( dma_PeriphConf[ dmaBus ].DmaReg,
                                         dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg );

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
 * \note Size different from the peripheral transfer size is handled by the
 *       channel (data packing / unpacking, RM0440).
 *
 * \param dmaBus             [in]: DMA peripheral ID
 * \param dmaChannel         [in]: DMA channel ID
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
        ( DMA_CHANNEL_CNT       > dmaChannel         ) &&
        ( DMA_TRANSFER_SIZE_CNT > memoryTransferSize )    )
    {
        if( 0u != LL_DMA_IsEnabledChannel( dma_PeriphConf[ dmaBus ].DmaReg,
                                          dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg ) )
        {
            /* Configuration is write protected while the channel is enabled */
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
                                  dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg,
                                  reqValue );

            for( uint32_t iterationCnt = 0u; DMA_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
            {
                regValue = LL_DMA_GetMemorySize( dma_PeriphConf[ dmaBus ].DmaReg,
                                                 dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg );

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
 * \param dmaChannel          [in]: DMA channel ID
 * \param memoryTransferSize [out]: Memory transfer size.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Get_MemoryTransferSize( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_TransferSize_t * const memoryTransferSize )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;
    uint32_t           regValue = 0u;

    if( ( DMA_PERIPH_CNT   > dmaBus             ) &&
        ( DMA_CHANNEL_CNT  > dmaChannel         ) &&
        ( DMA_NULL_PTR    != memoryTransferSize )    )
    {
        regValue = LL_DMA_GetMemorySize( dma_PeriphConf[ dmaBus ].DmaReg,
                                         dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg );

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
 * \param dmaChannel [in]: DMA channel ID
 * \param dataCount  [in]: Count of data to be transferred (max \ref DMA_DATA_COUNT_MAX)
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Set_DataCount( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_DataCount_t dataCount )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;
    uint32_t           regValue = 0u;

    if( ( DMA_PERIPH_CNT      > dmaBus     ) &&
        ( DMA_CHANNEL_CNT     > dmaChannel ) &&
        ( DMA_DATA_COUNT_MAX >= dataCount  )    )
    {
        if( 0u != LL_DMA_IsEnabledChannel( dma_PeriphConf[ dmaBus ].DmaReg,
                                          dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg ) )
        {
            /* Configuration is write protected while the channel is enabled */
            retState = DMA_REQUEST_ERROR;
        }
        else
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
 * \param dmaChannel [in]: DMA channel ID
 * \param dataCount [out]: Count of data to be transferred
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Get_DataCount( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_DataCount_t * const dataCount )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;

    if( ( DMA_PERIPH_CNT   > dmaBus     ) &&
        ( DMA_CHANNEL_CNT  > dmaChannel ) &&
        ( DMA_NULL_PTR    != dataCount  )    )
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
 * \brief Configures DMA transfer trigger request ID (DMAMUX1 request routed to the channel)
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

    if( ( DMA_PERIPH_CNT   > dmaBus              ) &&
        ( DMA_CHANNEL_CNT  > dmaChannel          ) &&
        ( DMA_REQ_ID_MAX  >= (uint32_t)requestId )    )
    {
        if( 0u != LL_DMA_IsEnabledChannel( dma_PeriphConf[ dmaBus ].DmaReg,
                                           dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg ) )
        {
            /* Configuration is write protected while the channel is enabled */
            retState = DMA_REQUEST_ERROR;
        }
        else
        {
            DMAMUX_Channel_TypeDef * const muxReg = DMA_MUX_CHANNEL_REG( dmaBus, dmaChannel );

            MODIFY_REG( muxReg->CCR, DMAMUX_CxCR_DMAREQ_ID, (uint32_t)requestId );

            for( uint32_t iterationCnt = 0u; DMA_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
            {
                regValue = READ_BIT( muxReg->CCR, DMAMUX_CxCR_DMAREQ_ID );

                if( (uint32_t)requestId == regValue )
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
 * \brief Returns DMA transfer trigger request ID (DMAMUX1 request routed to the channel)
 *
 * \param dmaBus      [in]: DMA peripheral ID
 * \param dmaChannel  [in]: DMA channel ID
 * \param requestId  [out]: Identification of peripheral triggering transfer request
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Get_PeripheralRequest( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_PeriphReqId_t * const requestId )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;

    if( ( DMA_PERIPH_CNT   > dmaBus     ) &&
        ( DMA_CHANNEL_CNT  > dmaChannel ) &&
        ( DMA_NULL_PTR    != requestId  )    )
    {
        *requestId = (dma_PeriphReqId_t)READ_BIT( DMA_MUX_CHANNEL_REG( dmaBus, dmaChannel )->CCR, DMAMUX_CxCR_DMAREQ_ID );

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
    uint32_t           reqValue = 0u;

    if( ( DMA_PERIPH_CNT   > dmaBus     ) &&
        ( DMA_CHANNEL_CNT  > dmaChannel ) &&
        ( DMA_PRIORITY_CNT > priority   )    )
    {
        if( 0u != LL_DMA_IsEnabledChannel( dma_PeriphConf[ dmaBus ].DmaReg,
                                          dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg ) )
        {
            /* Configuration is write protected while the channel is enabled */
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

            LL_DMA_SetChannelPriorityLevel( dma_PeriphConf[ dmaBus ].DmaReg,
                                           dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg,
                                           reqValue );

            for( uint32_t iterationCnt = 0u; DMA_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
            {
                regValue = LL_DMA_GetChannelPriorityLevel( dma_PeriphConf[ dmaBus ].DmaReg,
                                                          dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg );

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
 * \brief Returns channel priority
 *
 * \param dmaBus     [in]: DMA peripheral ID
 * \param dmaChannel [in]: DMA channel ID
 * \param priority  [out]: Channel priority configuration
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Get_Priority( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_Priority_t * const priority )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;
    uint32_t           regValue = 0u;

    if( ( DMA_PERIPH_CNT   > dmaBus     ) &&
        ( DMA_CHANNEL_CNT  > dmaChannel ) &&
        ( DMA_NULL_PTR    != priority   )    )
    {
        regValue = LL_DMA_GetChannelPriorityLevel( dma_PeriphConf[ dmaBus ].DmaReg,
                                                  dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg );

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
 * \brief De-activates Interrupt Requests (IRQ) of selected channel.
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


/**
 * \brief Enables transfer complete interrupt of selected channel.
 *
 * \param dmaBus     [in]: DMA peripheral ID
 * \param dmaChannel [in]: DMA channel ID
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
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
 * \brief Disables transfer complete interrupt of selected channel.
 *
 * \param dmaBus     [in]: DMA peripheral ID
 * \param dmaChannel [in]: DMA channel ID
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
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
 * \brief Enables half transfer interrupt of selected channel.
 *
 * \param dmaBus     [in]: DMA peripheral ID
 * \param dmaChannel [in]: DMA channel ID
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
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
 * \brief Disables half transfer interrupt of selected channel.
 *
 * \param dmaBus     [in]: DMA peripheral ID
 * \param dmaChannel [in]: DMA channel ID
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
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
 * \brief Enables transfer error interrupt of selected channel.
 *
 * \param dmaBus     [in]: DMA peripheral ID
 * \param dmaChannel [in]: DMA channel ID
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
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
 * \brief Disables transfer error interrupt of selected channel.
 *
 * \param dmaBus     [in]: DMA peripheral ID
 * \param dmaChannel [in]: DMA channel ID
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
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
 * \brief Configures transfer complete callback of selected channel.
 *
 * \param dmaBus     [in]: DMA peripheral ID
 * \param dmaChannel [in]: DMA channel ID
 * \param irqHandler [in]: Callback (null pointer disables the callback)
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
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


/**
 * \brief Returns transfer complete callback of selected channel.
 *
 * \param dmaBus      [in]: DMA peripheral ID
 * \param dmaChannel  [in]: DMA channel ID
 * \param irqHandler [out]: Callback
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Get_TransferCompleteIsrHandler( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_IsrCallback * const irqHandler )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;

    if( ( DMA_PERIPH_CNT   > dmaBus     ) &&
        ( DMA_CHANNEL_CNT  > dmaChannel ) &&
        ( DMA_NULL_PTR    != irqHandler )    )
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
 * \brief Configures half transfer callback of selected channel.
 *
 * \param dmaBus     [in]: DMA peripheral ID
 * \param dmaChannel [in]: DMA channel ID
 * \param irqHandler [in]: Callback (null pointer disables the callback)
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
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


/**
 * \brief Returns half transfer callback of selected channel.
 *
 * \param dmaBus      [in]: DMA peripheral ID
 * \param dmaChannel  [in]: DMA channel ID
 * \param irqHandler [out]: Callback
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Get_HalfTransferIsrHandler( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_IsrCallback * const irqHandler )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;

    if( ( DMA_PERIPH_CNT   > dmaBus     ) &&
        ( DMA_CHANNEL_CNT  > dmaChannel ) &&
        ( DMA_NULL_PTR    != irqHandler )    )
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
 * \brief Configures transfer error callback of selected channel.
 *
 * \param dmaBus     [in]: DMA peripheral ID
 * \param dmaChannel [in]: DMA channel ID
 * \param irqHandler [in]: Callback (null pointer disables the callback)
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
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


/**
 * \brief Returns transfer error callback of selected channel.
 *
 * \param dmaBus      [in]: DMA peripheral ID
 * \param dmaChannel  [in]: DMA channel ID
 * \param irqHandler [out]: Callback
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
dma_RequestState_t Dma_Get_TransferErrorIsrHandler( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, dma_IsrCallback * const irqHandler )
{
    dma_RequestState_t retState = DMA_REQUEST_ERROR;

    if( ( DMA_PERIPH_CNT   > dmaBus     ) &&
        ( DMA_CHANNEL_CNT  > dmaChannel ) &&
        ( DMA_NULL_PTR    != irqHandler )    )
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
 * \brief Returns flags of selected channel aligned to channel 1 position
 *        (\ref DMA_FLAG_GI, \ref DMA_FLAG_TC, \ref DMA_FLAG_HT, \ref DMA_FLAG_TE)
 *
 * \param dmaBus     [in]: DMA peripheral ID
 * \param dmaChannel [in]: DMA channel ID
 * \return Active flags of the channel
 */
static uint32_t Dma_Get_Flags( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel )
{
    const uint32_t regValue = READ_REG( dma_PeriphConf[ dmaBus ].DmaReg->ISR );

    return ( ( regValue >> ( (uint32_t)dmaChannel * DMA_FLAG_CHANNEL_SHIFT ) ) & DMA_FLAG_ALL );
}


/**
 * \brief Clears flags of selected channel
 *
 * \note Device errata 2.3.1: \ref DMA_FLAG_GI must not be cleared while the channel
 *       is enabled - callers clear it only after the channel was disabled.
 *
 * \param dmaBus     [in]: DMA peripheral ID
 * \param dmaChannel [in]: DMA channel ID
 * \param flags      [in]: Flags aligned to channel 1 position (\ref DMA_FLAG_ALL)
 */
static void Dma_Clear_Flags( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel, uint32_t flags )
{
    WRITE_REG( dma_PeriphConf[ dmaBus ].DmaReg->IFCR, ( flags & DMA_FLAG_ALL ) << ( (uint32_t)dmaChannel * DMA_FLAG_CHANNEL_SHIFT ) );
}


/**
 * \brief Activates clock of the peripheral (DMA or DMAMUX1) if it is not active yet
 *
 * \param rccPeriphId [in]: RCC identification of the peripheral
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
static dma_RequestState_t Dma_Set_ClockActive( rcc_PeriphId_t rccPeriphId )
{
    dma_RequestState_t  retState           = DMA_REQUEST_ERROR;
    rcc_FunctionState_t rccActivationState = RCC_FUNCTION_INACTIVE;
    rcc_RequestState_t  rccRequestState    = Rcc_Get_PeriphState( rccPeriphId, &rccActivationState );

    if( RCC_REQUEST_OK != rccRequestState )
    {
        retState = DMA_REQUEST_ERROR;
    }
    else if( RCC_FUNCTION_ACTIVE == rccActivationState )
    {
        /* No clock activation needed */
        retState = DMA_REQUEST_OK;
    }
    else if( RCC_REQUEST_OK == Rcc_Set_PeriphActive( rccPeriphId ) )
    {
        retState = DMA_REQUEST_OK;
    }
    else
    {
        retState = DMA_REQUEST_ERROR;
    }

    return ( retState );
}

/* =========================== INTERRUPT HANDLERS =========================== */

/**
 * \brief Common interrupt handler of DMA channels.
 *
 * Flag is cleared and callback executed only for interrupts enabled on the
 * channel. Half transfer is processed before transfer complete (both flags can
 * be active in one interrupt of short transfer). Global flag GIFx is not
 * cleared - the channel is active (device errata 2.3.1).
 *
 * \param dmaBus     [in]: DMA peripheral ID
 * \param dmaChannel [in]: DMA channel ID
 */
static void Dma_GlobalIsrHandler( dma_PeriphId_t dmaBus, dma_ChannelId_t dmaChannel )
{
    uint32_t flagStatus = Dma_Get_Flags( dmaBus, dmaChannel );

    if( ( 0u != ( DMA_FLAG_HT & flagStatus ) ) &&
        ( 0u != LL_DMA_IsEnabledIT_HT( dma_PeriphConf[ dmaBus ].DmaReg,
                                       dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg ) ) )
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

    if( ( 0u != ( DMA_FLAG_TC & flagStatus ) ) &&
        ( 0u != LL_DMA_IsEnabledIT_TC( dma_PeriphConf[ dmaBus ].DmaReg,
                                       dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg ) ) )
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

    if( ( 0u != ( DMA_FLAG_TE & flagStatus ) ) &&
        ( 0u != LL_DMA_IsEnabledIT_TE( dma_PeriphConf[ dmaBus ].DmaReg,
                                       dma_PeriphConf[ dmaBus ].ChannelsConfig[ dmaChannel ].ChannelReg ) ) )
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
}


static void Dma_Dma1Channel1_IsrHandler( void )
{
    Dma_GlobalIsrHandler( DMA_PERIPH_1, DMA_CHANNEL_1 );
    __DSB();    /* Cortex-M4 erratum 838869 (ES0430 / ES0431 / ES0523 2.1.3): stores completed before the exception return */
}


static void Dma_Dma1Channel2_IsrHandler( void )
{
    Dma_GlobalIsrHandler( DMA_PERIPH_1, DMA_CHANNEL_2 );
    __DSB();    /* Cortex-M4 erratum 838869 (ES0430 / ES0431 / ES0523 2.1.3): stores completed before the exception return */
}


static void Dma_Dma1Channel3_IsrHandler( void )
{
    Dma_GlobalIsrHandler( DMA_PERIPH_1, DMA_CHANNEL_3 );
    __DSB();    /* Cortex-M4 erratum 838869 (ES0430 / ES0431 / ES0523 2.1.3): stores completed before the exception return */
}


static void Dma_Dma1Channel4_IsrHandler( void )
{
    Dma_GlobalIsrHandler( DMA_PERIPH_1, DMA_CHANNEL_4 );
    __DSB();    /* Cortex-M4 erratum 838869 (ES0430 / ES0431 / ES0523 2.1.3): stores completed before the exception return */
}


static void Dma_Dma1Channel5_IsrHandler( void )
{
    Dma_GlobalIsrHandler( DMA_PERIPH_1, DMA_CHANNEL_5 );
    __DSB();    /* Cortex-M4 erratum 838869 (ES0430 / ES0431 / ES0523 2.1.3): stores completed before the exception return */
}


static void Dma_Dma1Channel6_IsrHandler( void )
{
    Dma_GlobalIsrHandler( DMA_PERIPH_1, DMA_CHANNEL_6 );
    __DSB();    /* Cortex-M4 erratum 838869 (ES0430 / ES0431 / ES0523 2.1.3): stores completed before the exception return */
}


#if defined(DMA1_Channel7)
static void Dma_Dma1Channel7_IsrHandler( void )
{
    Dma_GlobalIsrHandler( DMA_PERIPH_1, DMA_CHANNEL_7 );
    __DSB();    /* Cortex-M4 erratum 838869 (ES0430 / ES0431 / ES0523 2.1.3): stores completed before the exception return */
}
#endif


#if defined(DMA1_Channel8)
static void Dma_Dma1Channel8_IsrHandler( void )
{
    Dma_GlobalIsrHandler( DMA_PERIPH_1, DMA_CHANNEL_8 );
    __DSB();    /* Cortex-M4 erratum 838869 (ES0430 / ES0431 / ES0523 2.1.3): stores completed before the exception return */
}
#endif


#if defined(DMA2)
static void Dma_Dma2Channel1_IsrHandler( void )
{
    Dma_GlobalIsrHandler( DMA_PERIPH_2, DMA_CHANNEL_1 );
    __DSB();    /* Cortex-M4 erratum 838869 (ES0430 / ES0431 / ES0523 2.1.3): stores completed before the exception return */
}


static void Dma_Dma2Channel2_IsrHandler( void )
{
    Dma_GlobalIsrHandler( DMA_PERIPH_2, DMA_CHANNEL_2 );
    __DSB();    /* Cortex-M4 erratum 838869 (ES0430 / ES0431 / ES0523 2.1.3): stores completed before the exception return */
}


static void Dma_Dma2Channel3_IsrHandler( void )
{
    Dma_GlobalIsrHandler( DMA_PERIPH_2, DMA_CHANNEL_3 );
    __DSB();    /* Cortex-M4 erratum 838869 (ES0430 / ES0431 / ES0523 2.1.3): stores completed before the exception return */
}


static void Dma_Dma2Channel4_IsrHandler( void )
{
    Dma_GlobalIsrHandler( DMA_PERIPH_2, DMA_CHANNEL_4 );
    __DSB();    /* Cortex-M4 erratum 838869 (ES0430 / ES0431 / ES0523 2.1.3): stores completed before the exception return */
}


static void Dma_Dma2Channel5_IsrHandler( void )
{
    Dma_GlobalIsrHandler( DMA_PERIPH_2, DMA_CHANNEL_5 );
    __DSB();    /* Cortex-M4 erratum 838869 (ES0430 / ES0431 / ES0523 2.1.3): stores completed before the exception return */
}


static void Dma_Dma2Channel6_IsrHandler( void )
{
    Dma_GlobalIsrHandler( DMA_PERIPH_2, DMA_CHANNEL_6 );
    __DSB();    /* Cortex-M4 erratum 838869 (ES0430 / ES0431 / ES0523 2.1.3): stores completed before the exception return */
}


#if defined(DMA1_Channel7)
static void Dma_Dma2Channel7_IsrHandler( void )
{
    Dma_GlobalIsrHandler( DMA_PERIPH_2, DMA_CHANNEL_7 );
    __DSB();    /* Cortex-M4 erratum 838869 (ES0430 / ES0431 / ES0523 2.1.3): stores completed before the exception return */
}
#endif


#if defined(DMA1_Channel8)
static void Dma_Dma2Channel8_IsrHandler( void )
{
    Dma_GlobalIsrHandler( DMA_PERIPH_2, DMA_CHANNEL_8 );
    __DSB();    /* Cortex-M4 erratum 838869 (ES0430 / ES0431 / ES0523 2.1.3): stores completed before the exception return */
}
#endif


#endif /* DMA2 */

/* ================================ TASKS =================================== */
