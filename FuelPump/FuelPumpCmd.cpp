/*
 * FuelPumpCmd.cpp
 *
 *  Created on: Jun 14, 2021
 *      Author: EdwinKaburu
 */

#include <string.h>
#include "fw_log.h"
#include "fw_assert.h"
#include "Console.h"
#include "FuelPumpCmd.h"
#include "FuelPumpInterface.h"

FW_DEFINE_THIS_FILE("FuelPumpCmd.cpp")

namespace APP {
static CmdStatus Test(Console &console, Evt const *e) {
	switch (e->sig) {
	case Console::CONSOLE_CMD: {
		console.PutStr("FuelPump Test\n\r");
		break;
	}
	}
	return CMD_DONE;
}

static CmdStatus Pay(Console &console, Evt const *e) {
	switch (e->sig) {
	case Console::CONSOLE_CMD: {
		Console::ConsoleCmd const &ind =
				static_cast<Console::ConsoleCmd const&>(*e);

		if (ind.Argc() >= 2) {
			Evt *evt = NULL;

			uint8_t payment = STRING_TO_NUM(ind.Argv(1), 0);

			uint8_t amount = STRING_TO_NUM(ind.Argv(2), 0);

			if (amount > 0) {
				switch (payment) {
				case 0:
					console.Print("Payment = Cash, Amount =  %d\n\r", amount);
					evt = new FuelPumpPaymentInd(FUEL_PUMP, CONSOLE,
							FuelPumpPaymentInd::CASH, amount);
					break;
				case 1:
					console.Print("Payment = Credit, Amount =  %d\n\r", amount);
					evt = new FuelPumpPaymentInd(FUEL_PUMP, CONSOLE,
							FuelPumpPaymentInd::CREDIT, amount);
					break;
				default:
					FW_ASSERT(false);
				}

			}
			Fw::Post(evt);
			break;
		}

		console.Print("fuel pay <# Credit or Cash> <# Amount>\n\r");

		return CMD_DONE;
	}

	}

	return CMD_CONTINUE;
}

static CmdStatus Grade(Console &console, Evt const *e) {

	switch (e->sig) {
	case Console::CONSOLE_CMD: {

		Console::ConsoleCmd const &ind =
				static_cast<Console::ConsoleCmd const&>(*e);
		if (ind.Argc() >= 1) {
			uint32_t grade = STRING_TO_NUM(ind.Argv(1), 0);

			console.Print("Grade = %d \n\r", grade);
			// -----------------  Post Event -----------------
			Evt *evt = new FuelPumpGradeReq(FUEL_PUMP, console.GetHsmn(),
					console.GenSeq(), grade);
			Fw::Post(evt);

			break;
		}

		console.Print("fuel grade <#grade> \n\r");

		return CMD_DONE;
	}
	case FUEL_PUMP_GRADE_CFM: {
		ErrorEvt const &cfm = ERROR_EVT_CAST(*e);
		console.PrintErrorEvt(&cfm);
		return CMD_DONE;
	}

	}

	return CMD_CONTINUE;
}

static CmdStatus PriceRate(Console &console, Evt const *e) {

	switch (e->sig) {
	case Console::CONSOLE_CMD: {
		Console::ConsoleCmd const &ind =
				static_cast<Console::ConsoleCmd const&>(*e);

		if (ind.Argc() >= 2) {

			uint8_t grade = STRING_TO_NUM(ind.Argv(1), 0);
			uint8_t p_rate = STRING_TO_NUM(ind.Argv(2), 0);

			if (p_rate > 0 && p_rate < 11) {

				float np_rate = p_rate / 10.00;

				console.Print("Grade = %d, Price Rate =  %.2f\n\r", grade, np_rate);

				Evt *evt = new FuelPumpCPriceRateReq(FUEL_PUMP,
						console.GetHsmn(), console.GenSeq(), grade, np_rate);

				Fw::Post(evt);
				break;
			}
		}

		console.Print("fuel prate <#Grade> <#price rate>\n\r");

		return CMD_DONE;
	}
	case FUEL_PUMP_CPRICERATE_CFM: {
		ErrorEvt const &cfm = ERROR_EVT_CAST(*e);
		console.PrintErrorEvt(&cfm);
		return CMD_DONE;
	}

	}

	return CMD_CONTINUE;
}

static CmdStatus GallonRate(Console &console, Evt const *e) {

	switch (e->sig) {
	case Console::CONSOLE_CMD: {
		Console::ConsoleCmd const &ind =
				static_cast<Console::ConsoleCmd const&>(*e);

		if (ind.Argc() >= 2) {
			uint8_t g_rate = STRING_TO_NUM(ind.Argv(1), 0);

			if (g_rate > 0 && g_rate < 11) {

				float ng_rate = g_rate / 10.00;
				console.Print("Main Gallon Rate %.2f\n\r", ng_rate);

				Evt *evt = new FuelPumpCGallonRateInd(FUEL_PUMP, CONSOLE,
						ng_rate);
				Fw::Post(evt);
				break;
			}


		}

		console.Print("fuel grate <#gallon rate>\n\r");

		return CMD_DONE;
	}

	}

	return CMD_CONTINUE;
}

static CmdStatus List(Console &console, Evt const *e);

static CmdHandler const cmdHandler[] = { { "test", Test, "Test Function", 0 }, {
		"pay", Pay, "Makes Payment", 0 }, { "grade", Grade,
		"Select Grade [ 87, 89, 91, 93]", 0 }, { "prate", PriceRate,
		"Grade Price Rate ", 0 }, { "grate", GallonRate,
		"Main Tank Gallons Rate ", 0 }, { "?", List, "List Commands", 0 }, };

static CmdStatus List(Console &console, Evt const *e) {
	return console.ListCmd(e, cmdHandler, ARRAY_COUNT(cmdHandler));
}

CmdStatus FuelPumpCmd(Console &console, Evt const *e) {
	return console.HandleCmd(e, cmdHandler, ARRAY_COUNT(cmdHandler));
}

}
