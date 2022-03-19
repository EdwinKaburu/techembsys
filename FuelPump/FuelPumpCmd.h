/*
 * FuelPumpCmd.h
 *
 *  Created on: Jun 14, 2021
 *      Author: EdwinKaburu
 */

#ifndef FUEL_PUMP_CMD_H
#define FUEL_PUMP_CMD_H


#include "ConsoleInterface.h"

namespace APP {

CmdStatus FuelPumpCmd(Console &console, Evt const *e);

} // namespace APP

#endif /* FUEL_PUMP_CMD_H */
