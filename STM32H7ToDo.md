* --Board pin definitions--
* Extra SPI definitions SPI5 & 6
* --Allow SPI to work with/without DMA to unaligned/aligned addresses--
* --SPI DMA - Fix to work when no recv buffer & DMA--
* --ADC calibration constants seem to be using 16bit samples but we expect 12bit--
* --SDIO card interface--
* --DMA memory buffers (re-enable dcache currently disabled)--
* --TMC2209 interface--
* TMC5160 interface
* Check neopixel timing (consider using dma)
* Sort out STM32F4 and STM32H7 defines consider having base STM32 define
* --WiFi interface--
* --Check clock timing--
* Enable 16bit ADC samples
* Use processor d-ram for stacks etc.
* Check UART code (consider using FIFOs)
* --Common STM32 Core?--
* Crash dump flash interface
* CAN support
* Investigate SD Card SPI mode high speed switching
