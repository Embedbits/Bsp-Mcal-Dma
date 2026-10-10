# DMA – Direct Memory Access  
MCAL Driver (`Dma_Lib`) – STM32F7 family

The **DMA module** provides a complete MCAL-level abstraction for configuring,
controlling, and monitoring STM32F7 DMA peripherals. It supports stream-level
configuration, peripheral request (channel) selection, transfer modes,
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
 ├── Dma_Types.h         Public types (streams, requests, configuration)
 ├── Dma.c / Dma.h       Implementation
 └── Tests/
      ├── UnitTests/          Host unit tests (RegMem, RCC / NVIC mocks)
      └── IntegrationTests/   On-target tests (NUCLEO-F722ZE, 32F746GDISCOVERY)
```

Only **Dma_Port.h** shall be used by application code.

Dependencies: `Ral_Lib` (LL drivers), `Rcc_Lib` (DMA clock), `Nvic_Lib`
(stream interrupts).

---

## 🔄 Peripheral → Stream → DMA Architecture

STM32F7 has no request multiplexer (DMAMUX). Every DMA peripheral (DMA1, DMA2)
has **8 streams**, every stream selects one of **8 channels** (16 on
STM32F76x / F77x). Peripheral request is fixed to a combination of DMA
peripheral, stream and channel by the request mapping table of the reference
manual (RM0431 / RM0385 / RM0410 tables "DMA1 request mapping" / "DMA2 request
mapping").

```
 +----------------------+
 |   Peripheral (e.g.)  |
 |  USART, SPI, ADC...  |
 +----------+-----------+
            |
            | DMA request (fixed wiring to stream + channel)
            v
   +--------+---------+
   |  Stream x        |
   |------------------|
   | Channel select   |   dma_PeriphReqId_t  (DMA_REQ_CHANNEL_0..7)
   | (CHSEL)          |
   +--------+---------+
            |
            v
   +--------+---------+
   |   DMA1 / DMA2    |   dma_PeriphId_t     (DMA_PERIPH_1 / DMA_PERIPH_2)
   |------------------|
   | Stream control   |   dma_ChannelId_t    (DMA_STREAM_0..7)
   | FIFO / direct    |
   | Interrupt logic  |
   +------------------+
```

Module "channel" (`dma_ChannelId_t`, parameter `dmaChannel`) represents the
**DMA stream**.

---

## 🚀 Features

- DMA1 / DMA2, streams 0 – 7  
- Peripheral ⇄ memory transfers, memory → memory transfers (**DMA2 only**)  
- Normal & circular modes  
- 8/16/32-bit transfer sizes, data packing through FIFO (sizes differ)  
- Peripheral & memory address increments  
- Priority levels  
- Peripheral request selection (stream channel)  
- Transfer complete, half transfer and transfer error interrupts with user callbacks  

---

## ⚠️ STM32F7 Specifics

| Topic | Behavior |
|-------|----------|
| Write protection | Stream configuration registers are write protected by hardware while the stream is enabled. All configuration setters return `DMA_REQUEST_ERROR` for enabled stream. `Dma_Init()` disables the stream first. |
| Stream enable | All flags of the stream are cleared before enable (required by the reference manual). `Dma_Set_TransferActive()` succeeds when the stream runs or the transfer has already completed, fails on transfer error. |
| End of transfer | In normal mode the stream is disabled by hardware at the end of transfer (`Dma_Get_TransferState()` reports inactive, data count is 0). |
| Abort | `Dma_Set_TransferInactive()` masks stream interrupts while disabling - the abort does not execute transfer complete callback (hardware sets TCIF on software disable). |
| FIFO | `Dma_Init()` enables FIFO (threshold full) when peripheral and memory transfer sizes differ, otherwise direct mode is used (direct mode uses peripheral size on both sides). |
| Data count | 16 bit (`DMA_DATA_COUNT_MAX` = 65535), in units of peripheral (source) transfer size. |
| Memory to memory | DMA2 only, peripheral address is source, memory address is destination, circular mode not allowed. |
| Interrupt handler | Processes only interrupts enabled on the stream, half transfer before transfer complete. |

---

# 📦 Public API Summary

Full API available in **Dma_Port.h**.

Includes:

- Initialization / default configuration  
- Transfer start/stop and state  
- Stream (channel) activation  
- Direction / mode / increments / transfer sizes  
- Peripheral & memory addresses  
- Data count  
- Priority  
- Peripheral request selection  
- NVIC interrupt activation, stream interrupt enabling/disabling  
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

Request mappings in the examples are valid for STM32F7 (RM0385 / RM0410 / RM0431),
check the reference manual of the used device.

## Example 1 — USART1 TX DMA (DMA2 stream 7, channel 4)

```c
dma_ConfigStruct_t cfg;
Dma_Get_DefaultConfig(&cfg);

cfg.DmaPeriphId          = DMA_PERIPH_2;
cfg.DmaChannel           = DMA_STREAM_7;
cfg.Direction            = DMA_DIR_MEMORY_TO_PERIPH;
cfg.TransferMode         = DMA_TRANSFER_MODE_NORMAL;
cfg.PeriphAddress        = (uint32_t)&USART1->TDR;
cfg.MemoryAddress        = (uint32_t)txBuffer;
cfg.PeriphAddrIncrement  = DMA_PERIPH_ADDR_STATIC;
cfg.MemoryAddrIncrement  = DMA_MEMORY_ADDR_INCREMENT;
cfg.PeriphTransferSize   = DMA_TRANSFER_SIZE_8BIT;
cfg.MemoryTransferSize   = DMA_TRANSFER_SIZE_8BIT;
cfg.DataCount            = TX_LEN;
cfg.PeripheralReqId      = DMA_REQ_CHANNEL_4;
cfg.Priority             = DMA_PRIORITY_HIGH;

cfg.TransferCompleteCallback = TxDone;
cfg.TransferErrorCallback    = TxError;

Dma_Init(&cfg);
Dma_Set_InterruptActive(DMA_PERIPH_2, DMA_STREAM_7);
Dma_Set_TransferCompleteIrqActive(DMA_PERIPH_2, DMA_STREAM_7);
Dma_Set_TransferErrorIrqActive(DMA_PERIPH_2, DMA_STREAM_7);
Dma_Set_TransferActive(DMA_PERIPH_2, DMA_STREAM_7);
```

---

## Example 2 — Memory to Memory (DMA2 only)

```c
dma_ConfigStruct_t  cfg;
dma_FunctionState_t state = DMA_FUNCTION_ACTIVE;

Dma_Get_DefaultConfig(&cfg);

cfg.DmaPeriphId         = DMA_PERIPH_2;
cfg.DmaChannel          = DMA_STREAM_0;
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
Dma_Set_TransferActive(DMA_PERIPH_2, DMA_STREAM_0);

/* Stream is disabled by hardware at the end of transfer */
while( DMA_FUNCTION_ACTIVE == state )
{
    Dma_Get_TransferState(DMA_PERIPH_2, DMA_STREAM_0, &state);
}
```

---

# 🚀 Advanced Examples

## 🟢 Example A: **Circular ADC Sampling (DMA2 stream 4, channel 0)**

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

    cfg.DmaPeriphId         = DMA_PERIPH_2;
    cfg.DmaChannel          = DMA_STREAM_4;
    cfg.Direction           = DMA_DIR_PERIPH_TO_MEMORY;
    cfg.TransferMode        = DMA_TRANSFER_MODE_CIRCULAR;
    cfg.PeriphAddress       = (uint32_t)&ADC1->DR;
    cfg.MemoryAddress       = (uint32_t)AdcBuffer;
    cfg.MemoryAddrIncrement = DMA_MEMORY_ADDR_INCREMENT;
    cfg.PeriphTransferSize  = DMA_TRANSFER_SIZE_16BIT;
    cfg.MemoryTransferSize  = DMA_TRANSFER_SIZE_16BIT;
    cfg.DataCount           = ADC_BUF_SIZE;
    cfg.PeripheralReqId     = DMA_REQ_CHANNEL_0;

    cfg.HalfTransferCallback     = OnAdcHalfFilled;   /* First half ready  */
    cfg.TransferCompleteCallback = OnAdcFilled;       /* Second half ready */

    Dma_Init(&cfg);
    Dma_Set_InterruptActive(DMA_PERIPH_2, DMA_STREAM_4);
    Dma_Set_HalfTransferIrqActive(DMA_PERIPH_2, DMA_STREAM_4);
    Dma_Set_TransferCompleteIrqActive(DMA_PERIPH_2, DMA_STREAM_4);
    Dma_Set_TransferActive(DMA_PERIPH_2, DMA_STREAM_4);
}
```

### 🧩 Use cases:
- Current measurement on SMPS  
- Motor control FOC loop  
- High-frequency ADC sampling  
- Logging sensors without CPU load  

---

## 🟣 Example B: **SPI1 DMA TX/RX (DMA2 stream 3 / stream 2, channel 3)**

SPI requires one stream for **TX** and one stream for **RX**, which allows
full-duplex DMA transfers. RX stream shall be started before TX stream.

```c
uint8_t txData[64];
uint8_t rxData[64];

void SetupSpiDma(void)
{
    dma_ConfigStruct_t txCfg, rxCfg;

    /* TX stream */
    Dma_Get_DefaultConfig(&txCfg);
    txCfg.DmaPeriphId         = DMA_PERIPH_2;
    txCfg.DmaChannel          = DMA_STREAM_3;
    txCfg.Direction           = DMA_DIR_MEMORY_TO_PERIPH;
    txCfg.PeriphAddress       = (uint32_t)&SPI1->DR;
    txCfg.MemoryAddress       = (uint32_t)txData;
    txCfg.MemoryAddrIncrement = DMA_MEMORY_ADDR_INCREMENT;
    txCfg.DataCount           = sizeof(txData);
    txCfg.PeripheralReqId     = DMA_REQ_CHANNEL_3;
    txCfg.TransferCompleteCallback = OnSpiTxDone;

    /* RX stream */
    Dma_Get_DefaultConfig(&rxCfg);
    rxCfg.DmaPeriphId         = DMA_PERIPH_2;
    rxCfg.DmaChannel          = DMA_STREAM_2;
    rxCfg.Direction           = DMA_DIR_PERIPH_TO_MEMORY;
    rxCfg.PeriphAddress       = (uint32_t)&SPI1->DR;
    rxCfg.MemoryAddress       = (uint32_t)rxData;
    rxCfg.MemoryAddrIncrement = DMA_MEMORY_ADDR_INCREMENT;
    rxCfg.DataCount           = sizeof(rxData);
    rxCfg.PeripheralReqId     = DMA_REQ_CHANNEL_3;
    rxCfg.TransferCompleteCallback = OnSpiRxDone;

    Dma_Init(&txCfg);
    Dma_Init(&rxCfg);

    Dma_Set_TransferActive(DMA_PERIPH_2, DMA_STREAM_2);   /* RX first */
    Dma_Set_TransferActive(DMA_PERIPH_2, DMA_STREAM_3);
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
+---------+                         +------------+
|  CPU    |                         | DMA stream |
+---------+                         +------------+
     |                                    |
     | Dma_Init (stream disabled,         |
     | channel select, config)            |
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
     | Normal mode: EN = 0 by hardware    |
     |<-----------------------------------|
```

---

# 🧪 Testing

| Test set | Location | Run |
|----------|----------|-----|
| Unit tests (host, registers emulated by RegMem, RCC / NVIC mocked) | `Tests/UnitTests` | `UnitTest` preset, `ctest -R "^Dma\."` |
| Integration tests (NUCLEO-F722ZE, 32F746GDISCOVERY, DMA2 memory to memory transfers, interrupts, transfer error, abort) | `Tests/IntegrationTests` | `IntegrationTest` preset with `INTEGRATION_TEST_BOARD`, `ctest -R "^Dma\."` |

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
