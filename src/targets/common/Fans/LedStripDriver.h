/*
 * LedStripDriver.h
 *
 *  Created on: 18 Jul 2018
 *      Author: David
 */

#ifndef SRC_FANS_LEDSTRIPDRIVER_H_
#define SRC_FANS_LEDSTRIPDRIVER_H_

#include "RepRapFirmware.h"

class GCodeBuffer;

namespace LedStripDriver
{
	void Init() noexcept;
	bool MustStopMovement(GCodeBuffer& gb) noexcept;											// return true if we must stop movement to handle this command
	GCodeResult SetColours(GCodeBuffer& gb, const StringRef& reply) THROWS(GCodeException);		// handle M150
	GCodeResult Configure(GCodeBuffer& gb, const StringRef& reply) THROWS(GCodeException);		// handle M950
};

#endif /* SRC_FANS_FANS_LEDSTRIPDRIVER_H_ */
