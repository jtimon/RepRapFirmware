/*
 * SpiDevice.hpp
 *
 * STM32 version of RRF SpiDevice
 * Author: Andy
 */

#include <Hardware/Spi/SpiDevice.h>

#include <Hardware/IoPorts.h>

#include "Core.h"
#include "SoftwareSPI.h"
#include "HardwareSPI.h"

constexpr uint32_t DefaultSharedSpiClockFrequency = 2000000;
constexpr uint32_t SpiTimeout = 10000;

// SpiDevice members

SpiDevice::SpiDevice(SSPChannel chan) noexcept
    : hardware(SPI::getSSPDevice(chan))
{
}

// SharedSpiClient members

void SpiDevice::Disable() const noexcept
{

}

void SpiDevice::Enable() const noexcept
{

}

// Wait for transmitter ready returning true if timed out
inline bool SpiDevice::waitForTxReady() const noexcept
{
	return false;
}

// Wait for transmitter empty returning true if timed out
inline bool SpiDevice::waitForTxEmpty() const noexcept
{
	return false;
}

// Wait for receive data available returning true if timed out
inline bool SpiDevice::waitForRxReady() const noexcept
{
	return false;
}

void SpiDevice::SetClockFrequencyAndMode(uint32_t freq, SpiMode mode) const noexcept
{
	if (hardware != nullptr)
    	hardware->configureDevice(8, (uint8_t)mode, freq); 
}

bool SpiDevice::TransceivePacket(const uint8_t* tx_data, uint8_t* rx_data, size_t len) const noexcept
{
	if (hardware != nullptr)
		return hardware->transceivePacket(tx_data, rx_data, len) == SPI_OK;
	else
	{
		debugPrintf("Warning: Attempt to use an undefined shared SPI device\n");
		return false;
	}
}

// End
