/**
 * \author Mr.Nobody
 * \file Dma_Port.h
 * \ingroup Dma
 * \brief Empty DMA interface for CMock on STM32H7R / STM32H7S (unit tests of other modules).
 *
 * STM32H7R / STM32H7S have no DMA1 / DMA2 streams - the Dma module is empty there and the
 * modules use the Gpdma module. Unit tests of modules with both DMA variants include both mocks
 * (the test runner takes every Mock*.h include of the test file regardless of #if, CMock does
 * not evaluate #if of the mocked header). On STM32H7R / H7S this header is mocked instead of
 * ../../../Dma_Port.h - the generated MockDma_Port.h has no function.
 */

#ifndef DMA_TESTS_UNITTESTS_STUB_DMA_PORT_H
#define DMA_TESTS_UNITTESTS_STUB_DMA_PORT_H

/* No prototypes - DMA streams do not exist on STM32H7R / STM32H7S */

#endif /* DMA_TESTS_UNITTESTS_STUB_DMA_PORT_H */
