* --Board pin definitions--
* Extra SPI definitions SPI5 & 6
* --Allow SPI to work with/without DMA to unaligned/aligned addresses--
* --SPI DMA - Fix to work when no recv buffer & DMA--
* --ADC calibration constants seem to be using 16bit samples but we expect 12bit--
* --SDIO card interface--
* --DMA memory buffers (re-enable dcache currently disabled)--
* --TMC2209 interface--
* Test TMC5160 interface
* Check neopixel timing (consider using dma)
* Sort out STM32F4 and STM32H7 defines consider having base STM32 define
* --WiFi interface--
* --Check clock timing--
* Enable 16bit ADC samples (currently 12 bit with oversampling to 14)
* Use processor d-ram for stacks etc.
* --Check UART code--
* Consider using FIFOs for UARTS
* --Common STM32 Core?--
* Crash dump flash interface
* CAN support
* Investigate SD Card SPI mode high speed switching
* Test SBC mode
* Build H7 IAP for SBC mode
* Sort out a H7 bootloader for Fly boards
* Check H7 timer allocation
* use QSPI for neoplixels
* Allow use of shared timeers per pin (find best match)
* Update hardware usage for H7