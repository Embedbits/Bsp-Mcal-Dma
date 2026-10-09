# DMA – Direct Memory Access  
MCAL Driver (`Dma_Lib`) – STM32G4 family

The **DMA module** provides a complete MCAL-level abstraction for configuring,
controlling, and monitoring STM32G4 DMA peripherals. It supports channel-level
configuration, peripheral request routing by DMAMUX1, transfer modes,
interrupts and user callbacks.

This module is shared across all branches. Each branch represents a different
STM32 MCU family, switching between families is done by checking out the
corresponding branch. Public interface (`Dma_Port.h`) is the same for all
families, types (`Dma_Types.h`) follow the hardware of the family.

---

## 🧩 Architecture Overview

```
Dma_Lib
 ├── Dma_Port.h          Public interface (only header used by application)
 ├── Dma_Types.h         Public types (channels, requests, configuration)
 ├── Dma.c / Dma.h       Implementation
 └── Tests/
      ├── UnitTests/          Host unit tests (RegMem, RCC / NVIC mocks)
      └── IntegrationTests/   On-target tests (NUCLEO-G474RE)
```

Only **Dma_Port.h** shall be used by application code.

Dependencies: `Ral_Lib` (LL drivers), `Rcc_Lib` (DMA and DMAMUX1 clock),
`Nvic_Lib` (channel interrupts).

---

## 🔄 Peripheral → DMAMUX1 → DMA Architecture

Every DMA peripheral (DMA1, DMA2) has **8 channels** (**6 channels** on
STM32G411xB, G431, G441). Request of any peripheral can be routed by the
request multiplexer DMAMUX1 to any channel. DMAMUX1 channels 0 .. N - 1 serve
DMA1 channels 1 .. N, DMAMUX1 channels N .. 2N - 1 serve DMA2 channels 1 .. N
(N = channels per DMA).

```
 +----------------------+
 |   Peripheral (e.g.)  |
 |  USART, SPI, ADC...  |
 +----------+-----------+
            |
            | DMA request (DMAMUX1 input)
            v
   +--------+---------+
   |     DMAMUX1      |   dma_PeriphReqId_t  (DMA_REQ_USART1_TX, ...)
   |------------------|
   | Request routing  |
   +--------+---------+
            |
            v
   +--------+---------+
   |   DMA1 / DMA2    |   dma_PeriphId_t     (DMA_PERIPH_1 / DMA_PERIPH_2)
   |------------------|
   | Channel control  |   dma_ChannelId_t    (DMA_CHANNEL_1..8)
   | Interrupt logic  |
   +------------------+
```

---

## 🚀 Features

- DMA1 / DMA2, channels 1 – 8 (1 – 6 on STM32G411xB, G431, G441)  
- Peripheral ⇄ memory transfers, memory → memory transfers (both DMA peripherals)  
- Normal & circular modes  
- 8/16/32-bit transfer sizes, different peripheral / memory sizes (zero extension / truncation)  
- Peripheral & memory address increments  
- Priority levels  
- Peripheral request routing by DMAMUX1 (requests of peripherals of the device only)  
- Transfer complete, half transfer and transfer error interrupts with user callbacks  

---

## ⚠️ STM32G4 Specifics

| Topic | Behavior |
|-------|----------|
| Write protection | Channel configuration registers are write protected by hardware while the channel is enabled. All configuration setters return `DMA_REQUEST_ERROR` for enabled channel. `Dma_Init()` disables the channel first. |
| Clocks | `Dma_Init()` activates clock of the DMA peripheral and of DMAMUX1. |
| DMAMUX1 channel | DMAMUX1 register map keeps 8 channel slots per DMA on every STM32G4: DMA1 channel n uses slot n - 1, DMA2 channel n slot 8 + n - 1. On the 6-channel devices (STM32G411xB, G431, G441) slots 6 / 7 of each DMA are not implemented (CMSIS `DMAMUX1_Channel6` = `DMAMUX1_BASE + 0x20`, bug AB#1172 - AB#964 was a wrong analysis, `LL_DMA_SetPeriphRequest` uses the same map). |
| Channel enable | All flags of the channel are cleared before enable. `Dma_Set_TransferActive()` succeeds when the channel runs or the transfer has already completed, fails on transfer error. |
| End of transfer | In normal mode the channel **stays enabled** at the end of transfer, the data count reaches 0 (`Dma_Get_DataCount()`). The channel is disabled by `Dma_Set_TransferInactive()` (or `Dma_Init()`). Transfer error disables the channel by hardware. |
| Abort | `Dma_Set_TransferInactive()` masks channel interrupts while disabling - callbacks are not executed for the aborted transfer, flags are cleared. |
| Data width | Different peripheral and memory sizes are handled by the channel (RM0440 "programmable data width": zero extension / truncation, no packing). |
| Data count | 16 bit (`DMA_DATA_COUNT_MAX` = 65535), in units of peripheral (source) transfer size. |
| Memory to memory | DMA1 and DMA2, peripheral address is source, memory address is destination. Circular mode is not allowed - `Dma_Set_TransferActive()` refuses the combination. |
| Interrupt handler | Processes only interrupts enabled on the channel, half transfer before transfer complete. |
| Device errata | ES0430 / ES0431 / ES0523 2.3.1 "DMA disable failure and error flag omission upon simultaneous transfer error and global flag clear": global flag GIFx is cleared only while the channel is disabled, the interrupt handler clears TCIFx / HTIFx / TEIFx only. DMAMUX synchronization / request generator errata do not apply (not used). |

---

# 📦 Public API Summary

Full API available in **Dma_Port.h**.

Includes:

- Initialization / default configuration  
- Transfer start/stop and state  
- Channel activation  
- Direction / mode / increments / transfer sizes  
- Peripheral & memory addresses  
- Data count  
- Priority  
- Peripheral request routing (DMAMUX1)  
- NVIC interrupt activation, channel interrupt enabling/disabling  
- Callback registration  

---

# ⚙️ Configuration Structure

```c
typedef struct
{
    dma_PeriphId_t      DmaPeriphId;
    dma_ChannelId_t     DmaChannel;
    dma_Direction_t     Direction;
    dma_TransferMode_t  TransferMode;
    dma_PeriphAddr_t    PeriphAddress;
    dma_MemoryAddr_t    MemoryAddress;
    dma_PeriphAddrInc_t PeriphAddrIncrement;
    dma_MemoryAddrInc_t MemoryAddrIncrement;
    dma_TransferSize_t  PeriphTransferSize;
    dma_TransferSize_t  MemoryTransferSize;
    dma_DataCount_t     DataCount;
    dma_PeriphReqId_t   PeripheralReqId;
    dma_Priority_t      Priority;
    dma_IsrCallback     TransferCompleteCallback;
    dma_IsrCallback     HalfTransferCallback;
    dma_IsrCallback     TransferErrorCallback;
} dma_ConfigStruct_t;
```

---

# 🧪 Example Initialization

## Example 1 — USART1 TX DMA (DMA1 channel 1)

```c
dma_ConfigStruct_t cfg;
Dma_Get_DefaultConfig(&cfg);

cfg.DmaPeriphId          = DMA_PERIPH_1;
cfg.DmaChannel           = DMA_CHANNEL_1;
cfg.Direction            = DMA_DIR_MEMORY_TO_PERIPH;
cfg.TransferMode         = DMA_TRANSFER_MODE_NORMAL;
cfg.PeriphAddress        = (uint32_t)&USART1->TDR;
cfg.MemoryAddress        = (uint32_t)txBuffer;
cfg.PeriphAddrIncrement  = DMA_PERIPH_ADDR_STATIC;
cfg.MemoryAddrIncrement  = DMA_MEMORY_ADDR_INCREMENT;
cfg.PeriphTransferSize   = DMA_TRANSFER_SIZE_8BIT;
cfg.MemoryTransferSize   = DMA_TRANSFER_SIZE_8BIT;
cfg.DataCount            = TX_LEN;
cfg.PeripheralReqId      = DMA_REQ_USART1_TX;
cfg.Priority             = DMA_PRIORITY_HIGH;

cfg.TransferCompleteCallback = TxDone;
cfg.TransferErrorCallback    = TxError;

Dma_Init(&cfg);
Dma_Set_InterruptActive(DMA_PERIPH_1, DMA_CHANNEL_1);
Dma_Set_TransferCompleteIrqActive(DMA_PERIPH_1, DMA_CHANNEL_1);
Dma_Set_TransferErrorIrqActive(DMA_PERIPH_1, DMA_CHANNEL_1);
Dma_Set_TransferActive(DMA_PERIPH_1, DMA_CHANNEL_1);
```

---

## Example 2 — Memory to Memory

```c
dma_ConfigStruct_t cfg;
dma_DataCount_t    remaining = 1;

Dma_Get_DefaultConfig(&cfg);

cfg.DmaPeriphId         = DMA_PERIPH_2;
cfg.DmaChannel          = DMA_CHANNEL_1;
cfg.Direction           = DMA_DIR_MEMORY_TO_MEMORY;
cfg.TransferMode        = DMA_TRANSFER_MODE_NORMAL;
cfg.PeriphAddress       = (uint32_t)src;          /* Source      */
cfg.MemoryAddress       = (uint32_t)dst;          /* Destination */
cfg.PeriphAddrIncrement = DMA_PERIPH_ADDR_INCREMENT;
cfg.MemoryAddrIncrement = DMA_MEMORY_ADDR_INCREMENT;
cfg.PeriphTransferSize  = DMA_TRANSFER_SIZE_32BIT;
cfg.MemoryTransferSize  = DMA_TRANSFER_SIZE_32BIT;
cfg.DataCount           = 256;
cfg.PeripheralReqId     = DMA_REQ_MEM2MEM;

Dma_Init(&cfg);
Dma_Set_TransferActive(DMA_PERIPH_2, DMA_CHANNEL_1);

/* Channel stays enabled at the end of transfer - data count reaches 0 */
while( 0u != remaining )
{
    Dma_Get_DataCount(DMA_PERIPH_2, DMA_CHANNEL_1, &remaining);
}

Dma_Set_TransferInactive(DMA_PERIPH_2, DMA_CHANNEL_1);
```

---

# 🚀 Advanced Examples

## 🟢 Example A: **Circular ADC Sampling (DMA1 channel 2)**

Circular DMA mode restarts the transfer automatically once it reaches the end.
Half transfer and transfer complete callbacks allow processing of one half of
the buffer while DMA fills the other half (ping-pong).

```c
#define ADC_BUF_SIZE 256
uint16_t AdcBuffer[ADC_BUF_SIZE];

void SetupCircularAdcDma(void)
{
    dma_ConfigStruct_t cfg;
    Dma_Get_DefaultConfig(&cfg);

    cfg.DmaPeriphId         = DMA_PERIPH_1;
    cfg.DmaChannel          = DMA_CHANNEL_2;
    cfg.Direction           = DMA_DIR_PERIPH_TO_MEMORY;
    cfg.TransferMode        = DMA_TRANSFER_MODE_CIRCULAR;
    cfg.PeriphAddress       = (uint32_t)&ADC1->DR;
    cfg.MemoryAddress       = (uint32_t)AdcBuffer;
    cfg.MemoryAddrIncrement = DMA_MEMORY_ADDR_INCREMENT;
    cfg.PeriphTransferSize  = DMA_TRANSFER_SIZE_16BIT;
    cfg.MemoryTransferSize  = DMA_TRANSFER_SIZE_16BIT;
    cfg.DataCount           = ADC_BUF_SIZE;
    cfg.PeripheralReqId     = DMA_REQ_ADC1;

    cfg.HalfTransferCallback     = OnAdcHalfFilled;   /* First half ready  */
    cfg.TransferCompleteCallback = OnAdcFilled;       /* Second half ready */

    Dma_Init(&cfg);
    Dma_Set_InterruptActive(DMA_PERIPH_1, DMA_CHANNEL_2);
    Dma_Set_HalfTransferIrqActive(DMA_PERIPH_1, DMA_CHANNEL_2);
    Dma_Set_TransferCompleteIrqActive(DMA_PERIPH_1, DMA_CHANNEL_2);
    Dma_Set_TransferActive(DMA_PERIPH_1, DMA_CHANNEL_2);
}
```

### 🧩 Use cases:
- Current measurement on SMPS  
- Motor control FOC loop  
- High-frequency ADC sampling  
- Logging sensors without CPU load  

---

## 🟣 Example B: **SPI1 DMA TX/RX (DMA2 channel 1 / channel 2)**

SPI requires one channel for **TX** and one channel for **RX**, which allows
full-duplex DMA transfers. RX channel shall be started before TX channel.

```c
uint8_t txData[64];
uint8_t rxData[64];

void SetupSpiDma(void)
{
    dma_ConfigStruct_t txCfg, rxCfg;

    /* TX channel */
    Dma_Get_DefaultConfig(&txCfg);
    txCfg.DmaPeriphId         = DMA_PERIPH_2;
    txCfg.DmaChannel          = DMA_CHANNEL_1;
    txCfg.Direction           = DMA_DIR_MEMORY_TO_PERIPH;
    txCfg.PeriphAddress       = (uint32_t)&SPI1->DR;
    txCfg.MemoryAddress       = (uint32_t)txData;
    txCfg.MemoryAddrIncrement = DMA_MEMORY_ADDR_INCREMENT;
    txCfg.DataCount           = sizeof(txData);
    txCfg.PeripheralReqId     = DMA_REQ_SPI1_TX;
    txCfg.TransferCompleteCallback = OnSpiTxDone;

    /* RX channel */
    Dma_Get_DefaultConfig(&rxCfg);
    rxCfg.DmaPeriphId         = DMA_PERIPH_2;
    rxCfg.DmaChannel          = DMA_CHANNEL_2;
    rxCfg.Direction           = DMA_DIR_PERIPH_TO_MEMORY;
    rxCfg.PeriphAddress       = (uint32_t)&SPI1->DR;
    rxCfg.MemoryAddress       = (uint32_t)rxData;
    rxCfg.MemoryAddrIncrement = DMA_MEMORY_ADDR_INCREMENT;
    rxCfg.DataCount           = sizeof(rxData);
    rxCfg.PeripheralReqId     = DMA_REQ_SPI1_RX;
    rxCfg.TransferCompleteCallback = OnSpiRxDone;

    Dma_Init(&txCfg);
    Dma_Init(&rxCfg);

    Dma_Set_TransferActive(DMA_PERIPH_2, DMA_CHANNEL_2);   /* RX first */
    Dma_Set_TransferActive(DMA_PERIPH_2, DMA_CHANNEL_1);
}
```

### 🧩 Use cases:
- SPI screen refresh  
- High-speed sensors (IMU, gyroscope)  
- Flash memory read/write  
- Full-duplex communication (e.g., W5500 Ethernet chip)  

---

# 📐 UML Sequence – DMA Transfer Flow

```
+---------+                         +-------------+
|  CPU    |                         | DMA channel |
+---------+                         +-------------+
     |                                    |
     | Dma_Init (channel disabled,        |
     | DMAMUX1 request, config)           |
     |----------------------------------->|
     |                                    |
     | Dma_Set_TransferActive             |
     | (clear flags, EN = 1)              |
     |----------------------------------->|
     |                                    |
     |        Transfer Starts             |
     |<-----------------------------------|
     |                                    |
     |   Half Transfer Interrupt (HT)     |
     |<-----------------------------------|
     | Process first half buffer          |
     |                                    |
     | Transfer Complete Interrupt (TC)   |
     |<-----------------------------------|
     | Process second half buffer         |
     |                                    |
     | Normal mode: CNDTR = 0, EN kept    |
     | Dma_Set_TransferInactive (EN = 0)  |
     |----------------------------------->|
```

---

# 🧪 Testing

| Test set | Location | Run |
|----------|----------|-----|
| Unit tests (host, registers emulated by RegMem, RCC / NVIC mocked) | `Tests/UnitTests` | `UnitTest` preset, `ctest -R "^Dma\."` |
| Integration tests (NUCLEO-G474RE, DMA1 / DMA2 memory to memory transfers, interrupts, transfer error, abort, DMAMUX1 routing) | `Tests/IntegrationTests` | `IntegrationTest` preset with `INTEGRATION_TEST_BOARD`, `ctest -R "^Dma\."` |

---

# 🛠 CMake Integration

The module is created by `CMakeLists.txt` of the module (`Dma_Lib` static
library, public headers `Dma_Port.h` and `Dma_Types.h`).

Usage:

```cmake
target_link_libraries(App PRIVATE Dma_Lib)
```

---

## License

This project is licensed under the **Creative Commons Attribution–NonCommercial 4.0 International (CC BY-NC 4.0)**.

You are free to use, modify, and share this work for **non-commercial purposes**, provided appropriate credit is given.

See [LICENSE.md](LICENSE.md) for full terms or visit [creativecommons.org/licenses/by-nc/4.0](https://creativecommons.org/licenses/by-nc/4.0/).

---

## Authors

- **Mr.Nobody** — [embedbits.com](https://embedbits.com)

Contributions are welcome! Please open a pull request.

---

## 🌐 Useful Links

- [STM32CubeIDE](https://www.st.com/en/development-tools/stm32cubeide.html)
- [Azure DevOps](https://azure.microsoft.com/en-us/services/devops/)
- [Embedbits Github](https://github.com/Embedbits)
- [CC BY-NC 4.0 License](https://creativecommons.org/licenses/by-nc/4.0/)
