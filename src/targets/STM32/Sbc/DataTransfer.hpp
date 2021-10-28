//Author: sdavi

//SBC connected to SSP0
#include "Hardware/SharedSpi/SpiMode.h"
#include "HardwareSPI.h"
static HardwareSPI *spiDevice;

volatile bool dataReceived = false, transferReadyHigh = false;
volatile unsigned int spiTxUnderruns = 0, spiRxOverruns = 0;
enum SpiState {Disabled, Ready, Busy};
static volatile enum SpiState status = Disabled;

void InitSpi() noexcept;

// interrupt handler
void SpiInterrupt(HardwareSPI *spi) noexcept
{
    dataReceived = true;
    status = Ready;
    TaskBase::GiveFromISR(linuxTaskHandle);
}

static inline bool spi_dma_check_rx_complete() noexcept
{
    uint32_t startTime = millis();
    while (!digitalRead(SbcCsPin))			// transfer is complete if CS is high
    {
        RTOSIface::Yield();
        if (millis() - startTime > SpiTransferTimeout)
        {
            return false;
        }
    }
    return true;
}

void disable_spi()
{
    spiDevice->disable();
    status = Disabled;
}

void setup_spi(void *inBuffer, const void *outBuffer, size_t bytesToTransfer)
{
    if (status == Busy) disable_spi();
    if (status == Disabled)
    {
        InitSpi();
    }
    status = Busy;
    spiDevice->flushRx();
    spiDevice->startTransfer((const uint8_t *)outBuffer, (uint8_t *)inBuffer, bytesToTransfer, SpiInterrupt);
    // Begin transfer
    transferReadyHigh = !transferReadyHigh;
    digitalWrite(SbcTfrReadyPin, transferReadyHigh);
}

// Set up the SPI system
void InitSpi() noexcept
{
    spiDevice = (HardwareSPI *) SPI::getSSPDevice(SbcSpiChannel);
    spiDevice->configureDevice(SPI_MODE_SLAVE, 8, (uint8_t)SPI_MODE_0, 100000000, false);
    status = Ready;
}
