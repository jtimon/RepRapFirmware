* Board pin definitions
* Extra SPI definitions
* SPI DMA
* ADC calibration constants seem to be using 16bit samples but we expect 12bit
* SD card interface
* DMA memory buffers (re-enable dcache currently disabled)
* TMC2209 interface
* TMC5160 interface
* Check neopixel timing (consider using dma)
* Sort out STM32F4 and STM32H7 defines consider having base STM32 define
* WiFi interface
* Check clock timing
* Enable 16bit ADC samples
* Use processor d-ram for stacks etc.
* Check UART code (consider using FIFOs)
* Common STM32 Core?
* Crash dump flash interface
* CAN support
