/*
    Most STM32 boards use a Bootloader to flash new firmware. So firmware updates is just
    moving firmware.bin to / and rebooting.
*/
#if 0
void RepRap::PrepareToLoadIap() noexcept
{
#if SUPPORT_12864_LCD
	display->UpdatingFirmware();			// put the firmware update message on the display and stop polling the display
#endif

	// Send this message before we start using RAM that may contain message buffers
	platform->Message(AuxMessage, "Updating main firmware\n");
	platform->Message(UsbMessage, "Shutting down USB interface to update main firmware. Try reconnecting after 30 seconds.\n");

	// Allow time for the firmware update message to be sent
	const uint32_t now = millis();
	while (platform->FlushMessages() && millis() - now < 2000) { }

	// The machine will be unresponsive for a few seconds, don't risk damaging the heaters.
	// This also shuts down tasks and interrupts that might make use of the RAM that we are about to load the IAP binary into.
	EmergencyStop();						// this also stops Platform::Tick being called, which is necessary because it access Z probe object in RAM used by IAP
	network->Exit();						// kill the network task to stop it overwriting RAM that we use to hold the IAP
	SmartDrivers::Exit();					// stop the drivers being polled via SPI or UART because it may use data in the last 64Kb of RAM
	FilamentMonitor::Exit();				// stop the filament monitors generating interrupts, we may be about to overwrite them
	fansManager->Exit();					// stop the fan tachos generating interrupts, we may be about to overwrite them
#if SUPPORT_ACCELEROMETERS
	Accelerometers::Exit();					// terminate the accelerometer task, if any
#endif
	if (RTOSIface::GetCurrentTask() != Tasks::GetMainTask())
	{
		Tasks::TerminateMainTask();			// stop the main task if IAP is being written from another task
	}

#ifdef DUET_NG
	DuetExpansion::Exit();					// stop the DueX polling task
#endif
	StopAnalogTask();

	Cache::Disable();						// disable the cache because it interferes with flash memory access

#if USE_MPU
	ARM_MPU_Disable();						// make sure we can execute from RAM
#endif

#if 0
	// Debug
	memset(reinterpret_cast<char *>(IAP_IMAGE_START), 0x7E, 60 * 1024);
	delay(2000);
	for (char* p = reinterpret_cast<char *>(IAP_IMAGE_START); p < reinterpret_cast<char *>(IAP_IMAGE_START + (60 * 1024)); ++p)
	{
		if (*p != 0x7E)
		{
			debugPrintf("At %08" PRIx32 ": %02x\n", reinterpret_cast<uint32_t>(p), *p);
		}
	}
	debugPrintf("Scan complete\n");
	#endif
}
#endif

// Check the prerequisites for updating the main firmware. Return True if satisfied, else print a message to 'reply' and return false.
bool RepRap::CheckFirmwareUpdatePrerequisites(const StringRef& reply, const StringRef& filenameRef) noexcept
{
    #if HAS_MASS_STORAGE && (defined(LPC_NETWORKING) || HAS_WIFI_NETWORKING)
        FileStore * const firmwareFile = platform->OpenFile(FIRMWARE_DIRECTORY, filenameRef.IsEmpty() ? IAP_FIRMWARE_FILE : filenameRef.c_str(), OpenMode::read);
        if (firmwareFile == nullptr)
        {
            reply.printf("Firmware binary \"%s\" not found", FIRMWARE_FILE);
            return false;
        }

        // Check that the binary looks sensible. The first word is the initial stack pointer, which should be the top of RAM.
        const uint32_t initialStackPointer = 0x20020000-4;

        uint32_t firstDword;
        bool ok = firmwareFile->Read(reinterpret_cast<char*>(&firstDword), sizeof(firstDword)) == (int)sizeof(firstDword);
        firmwareFile->Close();
        if (!ok || firstDword != initialStackPointer)
        {
            reply.printf("Firmware binary \"%s\" is not valid for this electronics", FIRMWARE_FILE);
            return false;
        }
    #else
        reply.printf("To update firmware download firmware.bin to the sdcard as /firmware.bin and reset");
        return false;
    #endif

    return true;
}

// Update the firmware. Prerequisites should be checked before calling this.
void RepRap::UpdateFirmware(const StringRef& filenameRef) noexcept
{
#if  HAS_MASS_STORAGE && (defined(LPC_NETWORKING) || HAS_WIFI_NETWORKING)
    //DWC will upload firmware to 0:/sys/ we need to move to 0:/firmware.bin and reboot
    
    String<MaxFilenameLength> location;
    MassStorage::CombineName(location.GetRef(), FIRMWARE_DIRECTORY, filenameRef.IsEmpty() ? IAP_FIRMWARE_FILE : filenameRef.c_str());
    
    if(!MassStorage::Rename(location.c_str(), "0:/firmware.bin", true, false))
    {
        //failed to rename
        platform->MessageF(FirmwareUpdateMessage, "Failed to move firmware file.\n");
        return;
    }
    
    SoftwareReset(SoftwareResetReason::user); // Reboot
#endif
    
}
