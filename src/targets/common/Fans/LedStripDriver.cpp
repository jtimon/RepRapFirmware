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
	static uint32_t currentFrequency;									// the SPI frequency we are using
	static LedType ledType = DefaultLedType;
	static unsigned int numAlreadyInBuffer = 0;							// number of pixels already store in the buffer (NeoPixel only)
	static uint8_t *chunkBuffer = nullptr;								// buffer for sending data to LEDs
	static uint32_t whenOutputFinished = 0;								// the time in step clocks when we determined that the Output had finished
	static bool needStartFrame;											// true if we need to send a start frame with the next command
	static int32_t PixelTimings[4] = {350, 800, 1250, 250};
	static IoPort ledPort;

	static size_t MaxLedsPerBuffer() noexcept
	{
		switch (ledType)
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
			if (ledPort.IsValid())
			{
				Pin pin = ledPort.GetPin();
				const uint32_t T0H = NanosecondsToCycles(PixelTimings[0]);
				const uint32_t T0L = NanosecondsToCycles(PixelTimings[2] - PixelTimings[0]);
				const uint32_t T1H = NanosecondsToCycles(PixelTimings[1]);
				const uint32_t T1L = NanosecondsToCycles(PixelTimings[2] - PixelTimings[1]);
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
			numAlreadyInBuffer = 0;
			whenOutputFinished = StepTimer::GetTimerTicks();
			needStartFrame = true;
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
			if (ledPort.IsValid())
				NeopixelDMAWrite(ledPort.GetPin(), currentFrequency, chunkBuffer, p-chunkBuffer, PixelTimings[0], PixelTimings[1], 100);
			numAlreadyInBuffer = 0;
			whenOutputFinished = StepTimer::GetTimerTicks();
			needStartFrame = true;
		}

		return GCodeResult::ok;
	}
}

void LedStripDriver::Init() noexcept
{
	if (NeopixelOutPin != NoPin)
	{
        String<1> dummy;
        ledPort.AssignPort(GetPinNames(NeopixelOutPin), dummy.GetRef(), PinUsedBy::gpout, PinAccess::write0);
    }
	whenOutputFinished = StepTimer::GetTimerTicks();
	needStartFrame = true;
}

// Return true if we must stop movement before we handle this command
bool LedStripDriver::MustStopMovement(GCodeBuffer& gb) noexcept
{
#if SUPPORT_BITBANG_NEOPIXEL
	try
	{
		const LedType lt = (gb.Seen('X')) ? (LedType)gb.GetLimitedUIValue('X', 0, ARRAY_SIZE(LedTypeNames)) : ledType;
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
	if (needStartFrame
		&& ((StepTimer::GetTimerTicks() - whenOutputFinished) < (((uint32_t)PixelTimings[3] * StepClockRate)/1000000))
	   )
	{
		return GCodeResult::notFinished;									// give the NeoPixels time to reset
	}
	needStartFrame = false;

	// Deal with changing the LED type first
	bool seenType = false;
	bool needInit = false;
	if (gb.Seen('X'))
	{
		seenType = true;
		const uint32_t newType = gb.GetLimitedUIValue('X', 0, ARRAY_SIZE(LedTypeNames));
		const bool typeChanged = (newType != (unsigned int)ledType);

#if !SUPPORT_DMA_NEOPIXEL || !SUPPORT_BITBANG_NEOPIXEL
		// Check whether the new type is supported
		if (LedTypeNames[newType] == nullptr)
		{
			reply.copy("Unsupported LED strip type");
			return GCodeResult::error;
		}
#endif
#if SUPPORT_DMA_NEOPIXEL
# if SUPPORT_BITBANG_NEOPIXEL
		if (newType != (unsigned int)LedType::neopixelRGBBitBang && newType != (unsigned int)LedType::neopixelRGBWBitBang)
# endif
		{
			bool setFrequency = typeChanged;
			currentFrequency = (newType == (unsigned int)LedType::dotstar) ? DefaultDotStarSpiClockFrequency : DefaultNeoPixelSpiClockFrequency;
			gb.TryGetUIValue('Q', currentFrequency, setFrequency);
		}
#endif
		if (typeChanged)
		{
			ledType = (LedType)newType;
			needInit = true;
		}

		if (gb.Seen('T'))
		{
			size_t numTimings = ARRAY_SIZE(PixelTimings);
			gb.GetIntArray(PixelTimings, numTimings, true);
			if (numTimings != ARRAY_SIZE(PixelTimings))
			{
				reply.copy("bad timing parameter");
				return GCodeResult::error;
			}
		}
		if (gb.Seen('C'))
		{
			Pin pin = ledPort.GetPin();
			if (!ledPort.AssignPort(gb, reply, PinUsedBy::gpout, PinAccess::write0))
			{
				return GCodeResult::error;
			}
			needInit = ledPort.GetPin() != pin;
		}

	}

	if (needInit)
	{
		// Either we changed the type, or this is first-time initialisation
		numAlreadyInBuffer = 0;
		needInit = false;

#if SUPPORT_BITBANG_NEOPIXEL || SUPPORT_DMA_NEOPIXEL
		if (ledType == LedType::neopixelRGBBitBang || ledType == LedType::neopixelRGBWBitBang || ledType == LedType::neopixelRGB || ledType == LedType::neopixelRGBW)
		{
			// Set the data output low to start a WS2812 reset sequence
			if (ledPort.IsValid())
				ledPort.WriteDigital(false);
			whenOutputFinished = StepTimer::GetTimerTicks();
		}
#endif
		needStartFrame = true;
		return GCodeResult::notFinished;
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
		if (!seenType)
		{
			// Report the current configuration
			reply.printf("Led type is %s, timing %ld:%ld:%ld:%ld, ", LedTypeNames[(unsigned int)ledType], PixelTimings[0], PixelTimings[1], PixelTimings[2], PixelTimings[3]);
			ledPort.AppendBasicDetails(reply);
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

	switch (ledType)
	{
	case LedType::dotstar:
		break;

	case LedType::neopixelRGB:
	case LedType::neopixelRGBW:
#if SUPPORT_BITBANG_NEOPIXEL
		// Scale RGB by the brightness
		return DmaNeoPixelData(	(uint8_t)((red * brightness + 255) >> 8),
									(uint8_t)((green * brightness + 255) >> 8),
									(uint8_t)((blue * brightness + 255) >> 8),
									(uint8_t)((white * brightness + 255) >> 8),
									numLeds, (ledType == LedType::neopixelRGBWBitBang), following
							      );
#endif
		break;

	case LedType::neopixelRGBBitBang:
	case LedType::neopixelRGBWBitBang:
#if SUPPORT_BITBANG_NEOPIXEL
		// Scale RGB by the brightness
		return BitBangNeoPixelData(	(uint8_t)((red * brightness + 255) >> 8),
									(uint8_t)((green * brightness + 255) >> 8),
									(uint8_t)((blue * brightness + 255) >> 8),
									(uint8_t)((white * brightness + 255) >> 8),
									numLeds, (ledType == LedType::neopixelRGBWBitBang), following
							      );
#else
		break;
#endif
	}
	return GCodeResult::ok;													// should never get here
}

#endif

// End
