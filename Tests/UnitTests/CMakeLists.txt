#======================== Unit tests configuration ============================#
# Unit tests of Dma module. Registered in module CMakeLists.txt by
# UnitTesting_AddPath() and created by UnitTesting_Generate() in "UnitTest"
# build (see EmBi_Platform/CMake/UnitTesting/README.md).

get_filename_component(DMA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
get_filename_component(MCAL_DIR "${DMA_DIR}/.." ABSOLUTE)

# STM32H7R / STM32H7S have no DMA1 / DMA2 (module is empty) - tests only for the classic lines
if(NOT MCU_FAMILY_ID STREQUAL "STM32H7RSxx")
    UnitTesting_Add_Test(
        NAME            Dma
        TEST_SOURCE     Test_Dma.c
        UUT_SOURCES     ${DMA_DIR}/Dma.c
        MOCK_HEADERS    ${MCAL_DIR}/Rcc/Rcc_Port.h
                        ${MCAL_DIR}/Nvic/Nvic_Port.h
        INCLUDE_DIRS    ${DMA_DIR}
                        ${MCAL_DIR}/Rcc
                        ${MCAL_DIR}/Nvic
        LINK_LIBS       ut_RegMem                           # Registers in host memory
                        ut_CmsisHost                        # CMSIS core intrinsics on host
        FORCE_INCLUDES  ${UNIT_TESTING_CMSIS_HOST_HEADER}   # Replaces cmsis_gcc.h
        HOST_LIBS       Ral_Lib                             # ST LL compiled for host
        COVERAGE_EXCLUDE ".*/Ral/.*"
    )
endif()
