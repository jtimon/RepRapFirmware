/*
 * SharedSpiDevice.hpp
 *
 * LPC version of RRF SharedSpiDevice
 * Author: Andy
 */

#include <Hardware/IoPorts.h>

#include "Core.h"
#include "SoftwareSPI.h"
#include "HardwareSPI.h"
#include <Hardware/Spi/SharedSpiDevice.h>

constexpr uint32_t DefaultSharedSpiClockFrequency = 2000000;
constexpr uint32_t SpiTimeout = 10000;

// SharedSpiDevice members

SharedSpiDevice::SharedSpiDevice(SSPChannel chan) noexcept
    : SpiDevice(chan)
{
	mutex.Create("SPI");
}

// Static members

SharedSpiDevice *SharedSpiDevice::Devices[NumSPIDevices];
SharedSpiDevice *SharedSpiDevice::invalidDevice;

void SharedSpiDevice::Init() noexcept
{
	for(size_t i = 0; i < NumSPIDevices; i++)
		Devices[i] = new SharedSpiDevice((SSPChannel)i);
	invalidDevice = new SharedSpiDevice(SSPNONE);
}

// End
