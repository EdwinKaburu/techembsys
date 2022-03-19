/*
 * FuelTank.cpp
 *
 *  Created on: Jun 17, 2021
 *      Author: EdwinKaburu
 */

#include <FuelTank.h>

namespace APP {

// Default Main Tank with Grades along with Levels/Capacities/Rates.

FuelTank MAIN_TANK = {
		{
				{ 100.00, GRADE_87, 0.25 },
				{ 100.00, GRADE_89, 0.35 },
				{ 100.00, GRADE_91, 0.45 },
				{ 100.00, GRADE_93, 0.55 }
		},
		400.00, 0.10
};

}
