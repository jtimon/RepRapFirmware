/*******************************************************************************
SPI Driver:  
	Implementation

File Name:
    	CanSpi.cpp

Summary:
    	Implementation of MCU specific SPI functions.

Description:
	SPI Wrapper for MCP2517FD purposes.
 *******************************************************************************/

// Include files
#include "CanSpi.h"
#include "CoreImp.h"
#include "HardwareSPI.h"
#include "Platform/RepRap.h"

static HardwareSPI *spiDev;
extern "C" bool DRV_SPI_Initialize()
{
    if (CanCsPin == NoPin) return false;
    spiDev = &HardwareSPI::SSP3;
    pinMode(CanCsPin, OUTPUT_HIGH);
    spiDev->configureDevice(8, 0, 15000000);
    return true;
}


extern "C" int8_t DRV_SPI_TransferData(uint32_t index, uint8_t *SpiTxData, uint8_t *SpiRxData, size_t spiTransferSize)
{
    return spiDev->transceivePacket((const uint8_t *) SpiTxData, (uint8_t *)SpiRxData, spiTransferSize, CanCsPin);
}
