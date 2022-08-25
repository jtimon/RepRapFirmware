/*
 * LedStripDriver.cpp
 *
 *  Created on: 18 Jul 2018
 *      Author: David/GA
 * This is a simplified version of the standard Duet3D code that only supports NeoPixels
 * and which reduces the memory required.
 */

#include "LedStripDriver.h"

#if SUPPORT_LED_STRIPS

#include <GCodes/GCodeBuffer/GCodeBuffer.h>
#include <Movement/StepTimer.h>
#include <Platform/RepRap.h>
#include <GCodes/GCodes.h>
#include <DMABitIO.h>

namespace LedStripDriver
{
	constexpr uint32_t DefaultDotStarSpiClockFrequency = 1000000;		// 1MHz default
	constexpr uint32_t DefaultNeoPixelSpiClockFrequency = 2400000;		// must be between about 2MHz and about 4MHz

	constexpr size_t ChunkBufferSize = 180;								// the size of our buffer NeoPixels use 3 bytes per pixel
	enum class LedType : unsigned int
	{
		dotstar = 0,
		neopixelRGB,
		neopixelRGBBitBang,
		neopixelRGBW,
		neopixelRGBWBitBang
	};

	// In the following we set the text for the unavailable LED types to null, both to save flash memory and so we can test whether a type is supported
	constexpr const char *LedTypeNames[] =
	{
#if SUPPORT_DMA_DOTSTAR
		"DotStar on LED port",
#else
		nullptr,
#endif
#if SUPPORT_DMA_NEOPIXEL
		"NeoPixel RGB",
#else
		nullptr,
#endif
#if SUPPORT_BITBANG_NEOPIXEL
		"NeoPixel RGB bit-banged",
#else
		nullptr,
#endif
#if SUPPORT_DMA_NEOPIXEL
		"NeoPixel RGBW",
#else
		nullptr,
#endif
#if SUPPORT_BITBANG_NEOPIXEL
		"NeoPixel RGBW bit-banged"
#else
		nullptr
#endif
	};
#if SUPPORT_DMA_NEOPIXEL
	constexpr auto DefaultLedType = LedType::neopixelRGB;
#elif SUPPORT_BITBANG_NEOPIXEL
	constexpr auto DefaultLedType = LedType::neopixelRGBBitBang;
#endif
	constexpr int32_t DefaultPixelTimings[] = {350, 800, 1250, 250};

	static unsigned int numAlreadyInBuffer = 0;							// number of pixels already store in the buffer (NeoPixel only)
	static uint8_t *chunkBuffer = nullptr;								// buffer for sending data to LEDs
	static uint32_t whenOutputFinished = 0;								// the time in step clocks when we determined that the Output had finished
	static uint32_t startFrameTime = 0;								// how long in step clocks for the start frame
	static bool needStartFrame;											// true if we need to send a start frame with the next command
	static int32_t currentPort = -1;

	typedef struct
	{
		IoPort ledPort;
		int32_t pixelTimings[4];
		uint32_t frequency;
		LedType ledType;
	} LedPort;
	LedPort *ledPorts[MaxLedPorts];

	static size_t MaxLedsPerBuffer() noexcept
	{
		switch (ledPorts[currentPort]->ledType)
		{
		case LedType::dotstar:
		case LedType::neopixelRGBWBitBang:
			return ChunkBufferSize/4;

		case LedType::neopixelRGBW:
			return ChunkBufferSize/16;

		case LedType::neopixelRGBBitBang:
		case LedType::neopixelRGB:
			return ChunkBufferSize/3;

		//case LedType::neopixelRGB:
		default:
			return ChunkBufferSize/12;
		}
	}

	static void StartNeoPixelStartFrame()
	{
		LedPort &port = *ledPorts[currentPort];
		if (port.ledPort.IsValid())
			port.ledPort.WriteDigital(false);
		whenOutputFinished = StepTimer::GetTimerTicks();
		needStartFrame = true;
		startFrameTime = (((uint32_t)port.pixelTimings[3] * StepClockRate)/1000000);
		numAlreadyInBuffer = 0;
	}


	// Send data to NeoPixel LEDs by bit banging
	static GCodeResult BitBangNeoPixelData(uint8_t red, uint8_t green, uint8_t blue, uint8_t white, uint32_t numLeds, bool rgbw, bool following) noexcept
	{
		const unsigned int bytesPerLed = (rgbw) ? 4 : 3;
		uint8_t *p = chunkBuffer + (bytesPerLed * numAlreadyInBuffer);
		while (numLeds != 0 && p + bytesPerLed <= chunkBuffer + ChunkBufferSize)
		{
			*p++ = green;
			*p++ = red;
			*p++ = blue;
			if (rgbw)
			{
				*p++ = white;
			}
			--numLeds;
			++numAlreadyInBuffer;
		}

		if (!following)
		{
			LedPort &port = *ledPorts[currentPort];
			if (port.ledPort.IsValid())
			{
				Pin pin = port.ledPort.GetPin();
				const uint32_t T0H = NanosecondsToCycles(port.pixelTimings[0]);
				const uint32_t T0L = NanosecondsToCycles(port.pixelTimings[2] - port.pixelTimings[0]);
				const uint32_t T1H = NanosecondsToCycles(port.pixelTimings[1]);
				const uint32_t T1L = NanosecondsToCycles(port.pixelTimings[2] - port.pixelTimings[1]);
				const uint8_t *q = chunkBuffer;
				uint32_t nextDelay = T0L;
				IrqDisable();
				uint32_t lastTransitionTime = GetCurrentCycles();

				while (q < p)
				{
					uint8_t c = *q++;
					for (unsigned int i = 0; i < 8; ++i)
					{
						if (c & 0x80)
						{
							lastTransitionTime = DelayCycles(lastTransitionTime, nextDelay);
							fastDigitalWriteHigh(pin);
							__DSB();
							lastTransitionTime = DelayCycles(lastTransitionTime, T1H);
							fastDigitalWriteLow(pin);
							__DSB();
							nextDelay = T1L;
						}
						else
						{
							lastTransitionTime = DelayCycles(lastTransitionTime, nextDelay);
							fastDigitalWriteHigh(pin);
							__DSB();
							lastTransitionTime = DelayCycles(lastTransitionTime, T0H);
							fastDigitalWriteLow(pin);
							__DSB();
							nextDelay = T0L;
						}
						c <<= 1;
					}
				}
				IrqEnable();
			}
			StartNeoPixelStartFrame();
		}
		return GCodeResult::ok;
	}

	// Send data to NeoPixel LEDs by bit banging
	static GCodeResult DmaNeoPixelData(uint8_t red, uint8_t green, uint8_t blue, uint8_t white, uint32_t numLeds, bool rgbw, bool following) noexcept
	{
		const unsigned int bytesPerLed = (rgbw) ? 4 : 3;
		uint8_t *p = chunkBuffer + (bytesPerLed * numAlreadyInBuffer);
		while (numLeds != 0 && p + bytesPerLed <= chunkBuffer + ChunkBufferSize)
		{
			*p++ = green;
			*p++ = red;
			*p++ = blue;
			if (rgbw)
			{
				*p++ = white;
			}
			--numLeds;
			++numAlreadyInBuffer;
		}

		if (!following) 
		{
			LedPort &port = *ledPorts[currentPort];
			if (port.ledPort.IsValid())
				NeopixelDMAWrite(port.ledPort.GetPin(), port.frequency, chunkBuffer, p-chunkBuffer, port.pixelTimings[0], port.pixelTimings[1], 100);
			StartNeoPixelStartFrame();
		}

		return GCodeResult::ok;
	}
}

void LedStripDriver::Init() noexcept
{
	if (NeopixelOutPin != NoPin)
	{
        String<1> dummy;
		ledPorts[0] = new LedPort;
		LedPort &port = *ledPorts[0];
        port.ledPort.AssignPort(GetPinNames(NeopixelOutPin), dummy.GetRef(), PinUsedBy::gpout, PinAccess::write0);
		memcpy(port.pixelTimings, DefaultPixelTimings, sizeof(DefaultPixelTimings));
		port.frequency = DefaultNeoPixelSpiClockFrequency;
		port.ledType = DefaultLedType;
		currentPort = 0;
		StartNeoPixelStartFrame();
    }
}

// Return true if we must stop movement before we handle this command
bool LedStripDriver::MustStopMovement(GCodeBuffer& gb) noexcept
{
#if SUPPORT_BITBANG_NEOPIXEL
	try
	{
		const uint32_t ledPortNumber = gb.Seen('K') ? gb.GetLimitedUIValue('K', MaxLedPorts) : currentPort;
		const LedType lt = ledPortNumber >= 0 ? ledPorts[ledPortNumber]->ledType : DefaultLedType;
		return (lt == LedType::neopixelRGBBitBang || lt == LedType::neopixelRGBWBitBang) & gb.SeenAny("RUBWPYSF");
	}
	catch (const GCodeException&)
	{
		return true;
	}
#else
	return false;
#endif
}

// This function handles M950 operations for LED ports
GCodeResult LedStripDriver::Configure(GCodeBuffer& gb, const StringRef& reply) THROWS(GCodeException)
{
	const uint32_t ledPortNumber = gb.GetLimitedUIValue('E', MaxLedPorts);
	if (ledPorts[ledPortNumber] == nullptr)
		ledPorts[ledPortNumber] = new LedPort;
	LedPort &port = *ledPorts[ledPortNumber];
	bool seen = false;
	if (gb.Seen('C'))
	{
		if (!port.ledPort.AssignPort(gb, reply, PinUsedBy::gpout, PinAccess::write0))
		{
			return GCodeResult::error;
		}
		seen = true;
		memcpy(port.pixelTimings, DefaultPixelTimings, sizeof(DefaultPixelTimings));
		port.frequency = DefaultNeoPixelSpiClockFrequency;
		port.ledType = DefaultLedType;
	}
	if (gb.Seen('T'))
	{
		seen = true;
		const uint32_t newType = gb.GetLimitedUIValue('T', 0, ARRAY_SIZE(LedTypeNames));
		if (LedTypeNames[newType] == nullptr)
		{
			reply.copy("Unsupported LED strip type");
			return GCodeResult::error;
		}
		port.ledType = (LedType)newType;
	}
	if (gb.Seen('Q'))
	{
		seen = true;
		port.frequency = gb.GetLimitedUIValue('Q', 1000000, 4000000);
	}
	if (gb.Seen('L'))
	{
		size_t numTimings = ARRAY_SIZE(DefaultPixelTimings);
		gb.GetIntArray(port.pixelTimings, numTimings, true);
		if (numTimings != ARRAY_SIZE(DefaultPixelTimings))
		{
			reply.copy("bad timing parameter");
			return GCodeResult::error;
		}
	}
	if (!seen)
	{
		// Report the current configuration
		reply.printf("Led type is %s, timing %ld:%ld:%ld:%ld, frequency %u, ", LedTypeNames[(unsigned int)port.ledType], port.pixelTimings[0], port.pixelTimings[1], port.pixelTimings[2], port.pixelTimings[3], (unsigned)port.frequency);
		port.ledPort.AppendBasicDetails(reply);
	}
	return GCodeResult::ok;
}

// This function handles M150
// For DotStar LEDs:
// 	We can handle an unlimited length LED strip, because we can send the data in multiple chunks.
//	So whenever we receive a m150 command, we send the data immediately, in multiple chunks if our DMA buffer is too small to send it as a single chunk.
//	To send multiple chunks, we process the command once per chunk, using numRemaining to keep track of how many more LEDs need to be written to
// For NeoPixel LEDs:
//	If there is a gap or more then about 9us in transmission, the string will reset and the next command will be taken as applying to the start of the strip.
//  Therefore we need to DMA the data for all LEDs in one go. So the maximum strip length is limited by the size of our DMA buffer.
//	We buffer up incoming data until we get a command with the Following parameter missing or set to zero, then we DMA it all.
GCodeResult LedStripDriver::SetColours(GCodeBuffer& gb, const StringRef& reply) THROWS(GCodeException)
{
#if SUPPORT_BITBANG_NEOPIXEL
	// Interrupts are disabled while bit-banging data, so make sure movement has stopped if we are going to use bit-banging
	if (MustStopMovement(gb))
	{
		if (!reprap.GetGCodes().LockMovementAndWaitForStandstill(gb))
		{
			return GCodeResult::notFinished;
		}
	}
#endif
	// Handle start frame
	if (needStartFrame
		&& ((StepTimer::GetTimerTicks() - whenOutputFinished) < startFrameTime)
	   )
	{
		return GCodeResult::notFinished;									// give the NeoPixels time to reset
	}
	needStartFrame = false;
	bool seenPort = false;
	// Deal with changing the LED type first
	if (gb.Seen('K'))
	{
		const uint32_t ledPortNumber = gb.GetLimitedUIValue('K', MaxLedPorts);
		if (ledPorts[ledPortNumber] == nullptr || !ledPorts[ledPortNumber]->ledPort.IsValid())
		{
			reply.copy("Invalid led port");
			currentPort = -1;
			return GCodeResult::error;
		}
		if (currentPort != (int32_t)ledPortNumber)
		{
			numAlreadyInBuffer = 0;
			currentPort = ledPortNumber;
			StartNeoPixelStartFrame();
			return GCodeResult::notFinished;
		}
	}
	// Check to make sure we have a port selected
	if (currentPort < 0)
	{
		reply.copy("No led port selected");
		return GCodeResult::error;
	}

	// Get the RGB and brightness values
	uint32_t red = 0, green = 0, blue = 0, white = 0, brightness = 128;
	uint32_t numLeds = MaxLedsPerBuffer();
	bool following = false;
	bool seenColours = false;

	gb.TryGetLimitedUIValue('R', red, seenColours, 256);
	gb.TryGetLimitedUIValue('U', green, seenColours, 256);
	gb.TryGetLimitedUIValue('B', blue, seenColours, 256);
	gb.TryGetLimitedUIValue('W', white, seenColours, 256);					// W value is used by RGBW NeoPixels only

	if (gb.Seen('P'))
	{
		brightness = gb.GetLimitedUIValue('P', 256);						// valid P values are 0-255
	}
	else if (gb.Seen('Y'))
	{
		brightness = gb.GetLimitedUIValue('Y',  32) << 3;					// valid Y values are 0-31
	}

	gb.TryGetUIValue('S', numLeds, seenColours);
	gb.TryGetBValue('F', following, seenColours);

	if (!seenColours)
	{
		if (!seenPort)
		{
			// Report the current configuration
			reply.printf("Led port is %d", (int)currentPort);
		}
		return GCodeResult::ok;
	}

	// If there are no LEDs to set, we have finished unless we need to send a start frame to DotStar LEDs
	if (numLeds == 0)
	{
		return GCodeResult::ok;
	}
	
	// Allocate the buffer on first use
	if (chunkBuffer == nullptr)
		chunkBuffer = new uint8_t[ChunkBufferSize];

	// Apply brightness
	red = ((red * brightness + 255) >> 8);
	green = ((green * brightness + 255) >> 8);
	blue = ((blue * brightness + 255) >> 8);
	white = ((white * brightness + 255) >> 8);
	switch (ledPorts[currentPort]->ledType)
	{
	case LedType::dotstar:
		break;

	case LedType::neopixelRGB:
#if SUPPORT_DMA_NEOPIXEL
		// Scale RGB by the brightness
		return DmaNeoPixelData(	(uint8_t)red, (uint8_t)green, (uint8_t)blue, (uint8_t)white,
									numLeds, false, following
							      );
#else
		break;
#endif

	case LedType::neopixelRGBW:
#if SUPPORT_DMA_NEOPIXEL
		// Scale RGB by the brightness
		return DmaNeoPixelData(	(uint8_t)red, (uint8_t)green, (uint8_t)blue, (uint8_t)white,
									numLeds, true, following
							      );
#else
		break;
#endif

	case LedType::neopixelRGBBitBang:
#if SUPPORT_BITBANG_NEOPIXEL
		// Scale RGB by the brightness
		return BitBangNeoPixelData(	(uint8_t)red, (uint8_t)green, (uint8_t)blue, (uint8_t)white,
									numLeds, false, following
							      );
#else
		break;
#endif	
	case LedType::neopixelRGBWBitBang:
#if SUPPORT_BITBANG_NEOPIXEL
		// Scale RGB by the brightness
		return BitBangNeoPixelData(	(uint8_t)red, (uint8_t)green, (uint8_t)blue, (uint8_t)white,
									numLeds, true, following
							      );
#else
		break;
#endif
	}
	return GCodeResult::ok;													// should never get here
}

#endif

// End
