# Include current directory for provide headers include
set( Dma_IncludeDirs
    ${CMAKE_CURRENT_SOURCE_DIR} 
)


# Set source files for library generation
# All used function files shall be added here.
set( Dma_LibSrcs
    Dma.c )


# Set public header files for the library
# All header files used by other libraries shall be added here.
set( Dma_PublicHeaders 
    Dma_Port.h 
)


# Set dependent libraries
# All libraries used by current target shall be added here.
set( Dma_DependentLibs
    Ral_Lib
    Rcc_Lib
    Nvic_Lib
)


# Create library
add_library(Dma_Lib STATIC ${Dma_LibSrcs})


# Specify the public headers for the library
set_target_properties( Dma_Lib PROPERTIES PUBLIC_HEADER "${Dma_PublicHeaders}" )


# Include necessary libraries
target_link_libraries( Dma_Lib 
    PRIVATE 
    ${Dma_DependentLibs}
)


target_include_directories( Dma_Lib 
    INTERFACE 
    ${Dma_IncludeDirs}
)


message(STATUS "Dma_Lib library created")

#========================= Documentation configuration ========================#
if(DOXYGEN_AVAILABLE STREQUAL "ON")
    Doxygen_AddPath("${CMAKE_CURRENT_SOURCE_DIR}/")
endif()
