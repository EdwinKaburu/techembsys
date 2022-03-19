/*
 * FuelPump.cpp
 *
 *  Created on: Jun 14, 2021
 *      Author: EdwinKaburu
 */
#include "app_hsmn.h"
#include "fw_log.h"
#include "fw_assert.h"
#include "DispInterface.h"
#include "fw_active.h"
#include "GpioOutInterface.h"
#include "FuelPumpInterface.h"
#include "FuelPump.h"
#include "FuelTank.h"

FW_DEFINE_THIS_FILE("FuelPump.cpp")

namespace APP {

#undef ADD_EVT
#define ADD_EVT(e_) #e_,

static char const *const timerEvtName[] = { "FUEL_PUMP_TIMER_EVT_START",
FUEL_PUMP_TIMER_EVT };

static char const *const internalEvtName[] = { "FUEL_PUMP_INTERNAL_EVT_START",
FUEL_PUMP_INTERNAL_EVT };

static char const *const interfaceEvtName[] = { "FUEL_PUMP_INTERFACE_EVT_START",
FUEL_PUMP_INTERFACE_EVT };

static const char *DISMES_STRING[] = { "Insert Payment", "Select Grade",
		"Start Fueling", "Take Receipt", };

FuelPump::FuelPump() :
		Active((QStateHandler) &FuelPump::InitialPseudoState, FUEL_PUMP,
				"FUEL_PUMP"), m_price(0.00f), m_gallons(0.00f), m_max_amount(0), m_payment(
				0), m_paid(false), m_graded(false), m_isbtn(false), m_toUser(
		NULL), m_ingore(false), m_gallon_rate(0.00f), m_price_rate(0.00f), m_exit(
				false), m_currTank(MAIN_TANK), m_currGrade(
		NULL), m_timeoutTimer(GetHsm().GetHsmn(), TIME_OUT_TIMER), m_pumpTimer(
				GetHsm().GetHsmn(), PUMP_TIMER) {
	SET_EVT_NAME(FUEL_PUMP);
}

QState FuelPump::InitialPseudoState(FuelPump *const me, QEvt const *const e) {
	(void) e;
	return Q_TRAN(&FuelPump::Root);
}

QState FuelPump::Root(FuelPump *const me, QEvt const *const e) {
	switch (e->sig) {
	case Q_ENTRY_SIG: {
		EVENT(e);
		return Q_HANDLED();
	}
	case Q_EXIT_SIG: {
		EVENT(e);
		return Q_HANDLED();
	}
	case Q_INIT_SIG: {
		return Q_TRAN(&FuelPump::Stopped);
	}
	case FUEL_PUMP_START_REQ: {
		EVENT(e);
		Evt const &req = EVT_CAST(*e);
		Evt *evt = new FuelPumpStartCfm(req.GetFrom(), GET_HSMN(), req.GetSeq(),
				ERROR_STATE, GET_HSMN());
		Fw::Post(evt);
		return Q_HANDLED();
	}
	case FUEL_PUMP_STOP_REQ: {
		EVENT(e);
		me->GetHsm().Defer(e);
		return Q_TRAN(&FuelPump::Stopping);
	}

	}
	return Q_SUPER(&QHsm::top);
}

QState FuelPump::Stopped(FuelPump *const me, QEvt const *const e) {
	switch (e->sig) {
	case Q_ENTRY_SIG: {
		EVENT(e);

		return Q_HANDLED();
	}
	case Q_EXIT_SIG: {
		EVENT(e);
		return Q_HANDLED();
	}
	case FUEL_PUMP_STOP_REQ: {
		EVENT(e);
		Evt const &req = EVT_CAST(*e);
		Evt *evt = new FuelPumpStopCfm(req.GetFrom(), GET_HSMN(), req.GetSeq(),
				ERROR_SUCCESS);
		Fw::Post(evt);
		return Q_HANDLED();
	}
	case FUEL_PUMP_START_REQ: {
		EVENT(e);
		Evt const &req = EVT_CAST(*e);
		me->GetHsm().SaveInSeq(req);

		return Q_TRAN(&FuelPump::Starting);
	}
	}
	return Q_SUPER(&FuelPump::Root);
}

QState FuelPump::Starting(FuelPump *const me, QEvt const *const e) {
	switch (e->sig) {
	case Q_ENTRY_SIG: {
		EVENT(e);

		// Start Display

		me->GetHsm().ResetOutSeq();
		Evt *evt = new DispStartReq(ILI9341, GET_HSMN(), GEN_SEQ());
		me->GetHsm().SaveOutSeq(*evt);

		Fw::Post(evt);

		return Q_HANDLED();
	}
	case DISP_START_CFM: {
		EVENT(e);
		ErrorEvt const &cfm = ERROR_EVT_CAST(*e);
		bool Received;

		if (me->GetHsm().HandleCfmRsp(cfm, Received)) {
			Evt *evt = new Evt(DONE, GET_HSMN());
			me->PostSync(evt);
		}

		return Q_HANDLED();
	}
	case Q_EXIT_SIG: {
		EVENT(e);
		return Q_HANDLED();
	}
	case DONE: {
		EVENT(e);

		Evt *evt = new FuelPumpStartCfm(me->GetHsm().GetInHsmn(), GET_HSMN(),
				me->GetHsm().GetInSeq(), ERROR_SUCCESS);

		Fw::Post(evt);
		me->GetHsm().ClearInSeq();

		return Q_TRAN(&FuelPump::Started);
	}

	}
	return Q_SUPER(&FuelPump::Root);
}

QState FuelPump::Stopping(FuelPump *const me, QEvt const *const e) {

	switch (e->sig) {

	case Q_ENTRY_SIG: {
		EVENT(e);

		// Stop Display

		me->GetHsm().ResetOutSeq();
		Evt *evt = new DispStopReq(ILI9341, GET_HSMN(), GEN_SEQ());
		me->GetHsm().SaveOutSeq(*evt);
		Fw::Post(evt);

		return Q_HANDLED();
	}
	case Q_EXIT_SIG: {
		EVENT(e);
		me->GetHsm().Recall();
		return Q_HANDLED();
	}
	case FUEL_PUMP_STOP_REQ: {
		EVENT(e);
		me->GetHsm().Defer(e);
		return Q_HANDLED();
	}
	case DISP_STOP_CFM: {
		EVENT(e);
		ErrorEvt const &cfm = ERROR_EVT_CAST(*e);
		bool Received;

		if (me->GetHsm().HandleCfmRsp(cfm, Received)) {
			Evt *evt = new Evt(DONE, GET_HSMN());
			me->PostSync(evt);
		}

		return Q_HANDLED();
	}
	case DONE: {
		EVENT(e);
		return Q_TRAN(&FuelPump::Stopped);
	}

	}

	return Q_SUPER(&FuelPump::Root);
}

QState FuelPump::Started(FuelPump *const me, QEvt const *const e) {
	EVENT(e);
	switch (e->sig) {

	case Q_ENTRY_SIG: {
		EVENT(e);
		// IDRAW EVENT
		Evt *evt = new Evt(IDRAW, GET_HSMN());
		me->PostSync(evt);

		return Q_HANDLED();
	}
	case Q_EXIT_SIG: {
		EVENT(e);
		return Q_HANDLED();
	}
	case Q_INIT_SIG: {
		return Q_TRAN(&FuelPump::Idle);
	}

	}
	return Q_SUPER(&FuelPump::Root);
}

QState FuelPump::Idle(FuelPump *const me, QEvt const *const e) {

	switch (e->sig) {
	case Q_ENTRY_SIG: {
		EVENT(e);

		// Reset
		me->ValuesReset(me);

		// User Notification
		me->m_toUser = DISMES_STRING[IDLE_ENTRY];

		return Q_HANDLED();
	}
	case FUEL_PUMP_PAYMENT_IND: {
		EVENT(e);

		if (me->m_paid == false) {
			FuelPumpPaymentInd const &req =
					static_cast<FuelPumpPaymentInd const&>(*e);
			// Get user payment type
			me->m_payment = req.GetPayment();
			// Get user entered amount
			me->m_max_amount = req.GetAmount();

			me->m_paid = true;

			// 	User Notification
			me->m_toUser = DISMES_STRING[GRADE_ENTRY];
			//	Idle Draw Notification
			Evt *evt = new Evt(IDRAW, GET_HSMN());
			me->PostSync(evt);
		}

		return Q_HANDLED();

	}
	case FUEL_PUMP_CPRICERATE_REQ: {
		EVENT(e);

		// Assert m_paid = false
		if (me->m_paid == false) {
			FuelPumpCPriceRateReq const &req =
					static_cast<FuelPumpCPriceRateReq const&>(*e);

			FuelGrade *grade = me->m_currTank.GetFuelGradeG(
					(Grade) req.GetGradeType());

			Evt *evt = NULL;

			if (grade) {
				//  Set Price-Rate of this Grade
				grade->SetPriceRate(req.GetPriceRate());

				// Confirm Success
				evt = new FuelPumpCPriceRateCfm(req.GetFrom(), GET_HSMN(),
						req.GetSeq(), ERROR_SUCCESS);
				Fw::Post(evt);

			} else {

				// Confirm Error
				evt = new FuelPumpCPriceRateCfm(req.GetFrom(), GET_HSMN(),
						req.GetSeq(), ERROR_PARAM, GET_HSMN(),
						FUEL_PUMP_REASON_INVALID_GRADE);
				Fw::Post(evt);
			}

		}

		return Q_HANDLED();

	}
	case FUEL_PUMP_CGALLONRATE_IND: {
		EVENT(e);

		// Assert m_paid = false
		if (me->m_paid == false) {

			FuelPumpCGallonRateInd const &req =
					static_cast<FuelPumpCGallonRateInd const&>(*e);

			// Set/Change MainTank Gallon-Rate
			me->m_currTank.SetGallonsRate(req.GetGallonRate());

		}

		return Q_HANDLED();
	}
	case IDRAW: {
		// Transition to IdleDrawing
		return Q_TRAN(&FuelPump::IdleDrawing);
	}
	case Q_EXIT_SIG: {
		EVENT(e);
		return Q_HANDLED();
	}
	}
	return Q_SUPER(&FuelPump::Started);
}

QState FuelPump::IdleDrawing(FuelPump *const me, QEvt const *const e) {
	switch (e->sig) {

	case Q_ENTRY_SIG: {
		EVENT(e);

		me->m_exit = false;

		me->InitDraw(me);

		return Q_HANDLED();
	}
	case Q_EXIT_SIG: {
		EVENT(e);
		return Q_HANDLED();
	}
	case DISP_DRAW_END_CFM: {
		EVENT(e);

		// Assert m_exit = true
		if (me->m_exit) {
			// Assert m_paid = true
			if (me->m_paid) {

				//  WDRAW EVENT
				Evt *evt = new Evt(WDRAW, GET_HSMN());
				me->PostSync(evt);

				// Transition to Passive State
				return Q_TRAN(&FuelPump::Passive);
			}
			return Q_TRAN(&FuelPump::Idle);
		} else {

			Evt *evt = new DispDrawBeginReq(ILI9341, GET_HSMN(), GEN_SEQ());
			Fw::Post(evt);

			char buf[30];

			// User Notification
			evt = new DispDrawRectReq(ILI9341, GET_HSMN(), 0, 120, 160, 20,
			COLOR24_BLACK);
			Fw::Post(evt);

			snprintf(buf, sizeof(buf), me->m_toUser);
			evt = new DispDrawTextReq(ILI9341, GET_HSMN(), buf, 0, 120,
			COLOR24_RED, COLOR24_BLACK, 2);
			Fw::Post(evt);

			snprintf(buf, sizeof(buf), "87");
			evt = new DispDrawTextReq(ILI9341, GET_HSMN(), buf, 7, 265,
			COLOR24_WHITE, COLOR24_RED, 3);
			Fw::Post(evt);

			snprintf(buf, sizeof(buf), "89");
			evt = new DispDrawTextReq(ILI9341, GET_HSMN(), buf, 67, 265,
			COLOR24_WHITE, COLOR24_RED, 3);
			Fw::Post(evt);

			snprintf(buf, sizeof(buf), "91");
			evt = new DispDrawTextReq(ILI9341, GET_HSMN(), buf, 127, 265,
			COLOR24_WHITE, COLOR24_RED, 3);
			Fw::Post(evt);

			snprintf(buf, sizeof(buf), "93");
			evt = new DispDrawTextReq(ILI9341, GET_HSMN(), buf, 187, 265,
			COLOR24_WHITE, COLOR24_RED, 3);
			Fw::Post(evt);

			evt = new DispDrawEndReq(ILI9341, GET_HSMN(), GEN_SEQ());
			Fw::Post(evt);

			me->m_exit = true;
		}

		return Q_HANDLED();
	}

	}

	return Q_SUPER(&FuelPump::Started);
}

QState FuelPump::Passive(FuelPump *const me, QEvt const *const e) {
	switch (e->sig) {
	case Q_ENTRY_SIG: {
		EVENT(e);
		return Q_HANDLED();
	}
	case Q_INIT_SIG: {
		return Q_TRAN(&FuelPump::Waiting);
	}
	case Q_EXIT_SIG: {
		EVENT(e);
		return Q_HANDLED();
	}
	case TIME_OUT_TIMER: {
		EVENT(e);

		// Set m_paid to false
		//me->m_paid = false;

		// IDRAW EVENT
		Evt *evt = new Evt(IDRAW, GET_HSMN());
		me->PostSync(evt);

		// Transition to Idle State
		return Q_TRAN(&FuelPump::Idle);
	}

	}
	return Q_SUPER(&FuelPump::Started);
}

QState FuelPump::Waiting(FuelPump *const me, QEvt const *const e) {
	switch (e->sig) {

	case Q_ENTRY_SIG: {
		EVENT(e);
		// Timeout Start
		me->m_timeoutTimer.Start(ACTIVE_TIME_OUT);
		return Q_HANDLED();
	}
	case Q_EXIT_SIG: {
		EVENT(e);
		// Timeout Stop
		me->m_timeoutTimer.Stop();
		return Q_HANDLED();
	}
	case FUEL_PUMP_GRADE_REQ: {
		EVENT(e);

		// Assert both m_paid = true && m_graded = false
		if (me->m_paid && me->m_graded == false && me->m_ingore == false) {
			FuelPumpGradeReq const &req =
					static_cast<FuelPumpGradeReq const&>(*e);

			FuelGrade *grade = me->m_currTank.GetFuelGradeG(
					(Grade) req.GetGradeType());

			if (grade) {

				// Set currGrade to grade
				me->m_currGrade = grade;
				me->m_graded = true;

				me->m_ingore = true;

				// Get Grade Price-Rate
				me->m_price_rate = grade->GetPriceRate();
				// Get Gallon Price-Rate
				me->m_gallon_rate = me->m_currTank.GetGallonsRate();

				// Confirm Success
				Evt *evt = new FuelPumpGradeCfm(req.GetFrom(), GET_HSMN(),
						req.GetSeq(), ERROR_SUCCESS);
				Fw::Post(evt);

				// WDRAW EVENT
				evt = new Evt(WDRAW, GET_HSMN());
				me->PostSync(evt);

				//  User Notification
				me->m_toUser = DISMES_STRING[FUEL_ENTRY];

			} else {
				// Confirm Error : Invalid Grade
				Evt *evt = new FuelPumpGradeCfm(req.GetFrom(), GET_HSMN(),
						req.GetSeq(), ERROR_PARAM, GET_HSMN(),
						FUEL_PUMP_REASON_INVALID_GRADE);
				Fw::Post(evt);

			}

		}

		return Q_HANDLED();
	}
	case WDRAW: {
		// Transition to WaitingDrawing
		return Q_TRAN(&FuelPump::WaitingDrawing);
	}

	}

	return Q_SUPER(&FuelPump::Passive);
}

QState FuelPump::WaitingDrawing(FuelPump *const me, QEvt const *const e) {
	switch (e->sig) {
	case Q_ENTRY_SIG: {
		EVENT(e);

		me->WaitDraw(me);

		return Q_HANDLED();
	}
	case DISP_DRAW_END_CFM: {
		EVENT(e);

		// Assert m_graded = true
		if (me->m_graded) {

			return Q_TRAN(&FuelPump::Pumping);
		}
		return Q_TRAN(&FuelPump::Waiting);

	}

	case Q_EXIT_SIG: {
		EVENT(e);
		return Q_HANDLED();
	}

	}
	return Q_SUPER(&FuelPump::Passive);
}

QState FuelPump::Pumping(FuelPump *const me, QEvt const *const e) {
	switch (e->sig) {
	case Q_ENTRY_SIG: {
		EVENT(e);

		// Turn On LED,
		Evt *evt = new GpioOutPatternReq(USER_LED, GET_HSMN(), GEN_SEQ(), 0,
				true);
		Fw::Post(evt);

		return Q_HANDLED();
	}
	case Q_EXIT_SIG: {
		EVENT(e);

		// Turn Off LED
		Evt *evt = new GpioOutOffReq(USER_LED, GET_HSMN(), GEN_SEQ());
		Fw::Post(evt);

		return Q_HANDLED();
	}
	case Q_INIT_SIG: {
		return Q_TRAN(&FuelPump::Filling);
	}
	case PUMP_TIMER: {
		EVENT(e);

		// COMPLETE EVENT
		Evt *evt = new Evt(COMPLETE, GET_HSMN());
		me->PostSync(evt);

		// Transition to Admission
		return Q_TRAN(&FuelPump::Admission);
	}

	}
	return Q_SUPER(&FuelPump::Passive);
}

QState FuelPump::Filling(FuelPump *const me, QEvt const *const e) {
	switch (e->sig) {
	case Q_ENTRY_SIG: {
		EVENT(e);
		// Start PumpTimer
		me->m_pumpTimer.Start(PUMP_TIMER);

		return Q_HANDLED();
	}
	case Q_EXIT_SIG: {
		EVENT(e);
		// End PumpTimer
		me->m_pumpTimer.Stop();
		return Q_HANDLED();
	}
	case FUEL_PUMP_FILL_IND: {
		EVENT(e);

		if (me->m_isbtn == false) {

			// Calculate Price and Gallons
			if (me->m_gallons < me->m_currGrade->GetGradeCapacity() - 1
					&& me->m_price < me->m_max_amount) {

				if (me->m_price
						< (me->m_max_amount - me->m_price_rate) - 0.50f) {
					me->m_gallons += me->m_gallon_rate;
					me->m_price += me->m_price_rate;
				} else {
					me->m_gallons += 0.01f;
					me->m_price += 0.01f;
				}

				// FLDRAW EVENT
				Evt *evt = new Evt(FLDRAW, GET_HSMN());
				me->PostSync(evt);

				//me->m_pumpTimer.Restart(PUMP_TIMER);
				me->m_isbtn = true;

			}

		}

		return Q_HANDLED();
	}
	case FLDRAW: {

		EVENT(e);
		return Q_TRAN(&FuelPump::ReDrawing);

	}

	}
	return Q_SUPER(&FuelPump::Pumping);
}

QState FuelPump::ReDrawing(FuelPump *const me, QEvt const *const e) {
	switch (e->sig) {

	case Q_ENTRY_SIG: {
		EVENT(e);

		Evt *evt = new DispDrawBeginReq(ILI9341, GET_HSMN(), GEN_SEQ());
		Fw::Post(evt);
		char buf[30];

		snprintf(buf, sizeof(buf), "%.2f", me->m_price);
		evt = new DispDrawTextReq(ILI9341, GET_HSMN(), buf, 130, 10,
		COLOR24_WHITE, COLOR24_BLACK, 2);
		Fw::Post(evt);

		snprintf(buf, sizeof(buf), "%.2f", me->m_gallons);
		evt = new DispDrawTextReq(ILI9341, GET_HSMN(), buf, 130, 50,
		COLOR24_WHITE, COLOR24_BLACK, 2);
		Fw::Post(evt);

		evt = new DispDrawEndReq(ILI9341, GET_HSMN(), GEN_SEQ());
		Fw::Post(evt);

		return Q_HANDLED();
	}
	case Q_EXIT_SIG: {
		EVENT(e);
		return Q_HANDLED();
	}
	case DISP_DRAW_END_CFM: {
		EVENT(e);

		if (me->m_isbtn) {
			me->m_isbtn = false;
		}

		return Q_TRAN(&FuelPump::Filling);
	}

	}
	return Q_SUPER(&FuelPump::Pumping);
}

QState FuelPump::Admission(FuelPump *const me, QEvt const *const e) {
	switch (e->sig) {
	case Q_ENTRY_SIG: {
		EVENT(e);

		// Update Grade Capacity
		float new_capacity = me->m_currGrade->GetGradeCapacity()
				- me->m_gallons;

		me->m_currGrade->SetGradeCapacity(new_capacity);

		// Update Main Tank
		me->m_currTank.UpdateTankCapacity();

		// User Notification
		me->m_toUser = DISMES_STRING[ADMISSION_ENTRY];

		return Q_HANDLED();
	}
	case Q_EXIT_SIG: {
		EVENT(e);
		return Q_HANDLED();
	}
	case COMPLETE: {

		// Set m_graded to false
		me->m_graded = false;

		// WDRAW EVENT
		//Evt *evt = new Evt(WDRAW, GET_HSMN());
		//me->PostSync(evt);
		return Q_TRAN(&FuelPump::WaitingDrawing);

	}

	}
	return Q_SUPER(&FuelPump::Passive);

}

void FuelPump::InitDraw(FuelPump *const me) {

	Evt *evt = new DispDrawBeginReq(ILI9341, GET_HSMN(), GEN_SEQ());
	Fw::Post(evt);

	// -------------- Main Rectangle ----------
	evt = new DispDrawRectReq(ILI9341, GET_HSMN(), 0, 0, 240, 320,
	COLOR24_BLACK);
	Fw::Post(evt);

	char buf[30];

	snprintf(buf, sizeof(buf), "Price = ");
	evt = new DispDrawTextReq(ILI9341, GET_HSMN(), buf, 0, 10,
	COLOR24_WHITE, COLOR24_BLACK, 2);
	Fw::Post(evt);

	snprintf(buf, sizeof(buf), "%.2f", me->m_price);
	evt = new DispDrawTextReq(ILI9341, GET_HSMN(), buf, 130, 10,
	COLOR24_WHITE, COLOR24_BLACK, 2);
	Fw::Post(evt);

	// -------------- Gallons -----------------

	snprintf(buf, sizeof(buf), "Gallons = ");
	evt = new DispDrawTextReq(ILI9341, GET_HSMN(), buf, 0, 50,
	COLOR24_WHITE, COLOR24_BLACK, 2);

	Fw::Post(evt);

	snprintf(buf, sizeof(buf), "%.2f", me->m_gallons);
	evt = new DispDrawTextReq(ILI9341, GET_HSMN(), buf, 130, 50,
	COLOR24_WHITE, COLOR24_BLACK, 2);

	Fw::Post(evt);

	// Payment Type
	if (me->m_payment == 0) {

		snprintf(buf, sizeof(buf), "Payment = Cash");
		evt = new DispDrawTextReq(ILI9341, GET_HSMN(), buf, 0, 90,
		COLOR24_WHITE, COLOR24_BLACK, 2);

		Fw::Post(evt);
	} else if (me->m_payment == 1) {
		snprintf(buf, sizeof(buf), "Payment = Credit");
		evt = new DispDrawTextReq(ILI9341, GET_HSMN(), buf, 0, 90,
		COLOR24_WHITE, COLOR24_BLACK, 2);

		Fw::Post(evt);
	}

	evt = new DispDrawRectReq(ILI9341, GET_HSMN(), 0, 150, 110, 65,
	COLOR24_BLACK);
	Fw::Post(evt);

	snprintf(buf, sizeof(buf), "Capacity");
	evt = new DispDrawTextReq(ILI9341, GET_HSMN(), buf, 60, 150,
	COLOR24_BLACK, COLOR24_GRAY, 2);
	Fw::Post(evt);

	snprintf(buf, sizeof(buf), "Tank:");
	evt = new DispDrawTextReq(ILI9341, GET_HSMN(), buf, 0, 170,
	COLOR24_WHITE, COLOR24_BLACK, 2);
	Fw::Post(evt);

	snprintf(buf, sizeof(buf), "%.1f", me->m_currTank.GetTankCapacity());
	evt = new DispDrawTextReq(ILI9341, GET_HSMN(), buf, 0, 200,
	COLOR24_WHITE, COLOR24_BLACK, 2);
	Fw::Post(evt);

	// ------------- Grade 87 ---------------------
	evt = new DispDrawRectReq(ILI9341, GET_HSMN(), 0, 250, 50, 50,
	COLOR24_RED);
	Fw::Post(evt);

	// ------------- Grade 89 ---------------------
	evt = new DispDrawRectReq(ILI9341, GET_HSMN(), 60, 250, 50, 50,
	COLOR24_RED);
	Fw::Post(evt);

	// ------------- Grade 91 ---------------------
	evt = new DispDrawRectReq(ILI9341, GET_HSMN(), 120, 250, 50, 50,
	COLOR24_RED);
	Fw::Post(evt);

	//  ------------- Grade 93 ---------------------
	evt = new DispDrawRectReq(ILI9341, GET_HSMN(), 180, 250, 50, 50,
	COLOR24_RED);
	Fw::Post(evt);

	// ----------- End of Draw  -------------
	evt = new DispDrawEndReq(ILI9341, GET_HSMN(), GEN_SEQ());
	Fw::Post(evt);

}

void FuelPump::WaitDraw(FuelPump *const me) {

	Evt *evt = new DispDrawBeginReq(ILI9341, GET_HSMN(), GEN_SEQ());
	Fw::Post(evt);

	char buf[30];

	if (me->m_graded) {

		switch (me->m_currGrade->GetGrade()) {
		case 87:
			GradeDraw(me, me->m_currGrade->GetGrade(), buf, 0, 7);
			break;
		case 89:
			GradeDraw(me, me->m_currGrade->GetGrade(), buf, 60, 67);
			break;
		case 91:
			GradeDraw(me, me->m_currGrade->GetGrade(), buf, 120, 127);
			break;
		case 93:
			GradeDraw(me, me->m_currGrade->GetGrade(), buf, 180, 187);
			break;

		}

	}

	// Display Selected Grade Capacity

	evt = new DispDrawRectReq(ILI9341, GET_HSMN(), 120, 170, 120, 65,
	COLOR24_BLACK);
	Fw::Post(evt);

	snprintf(buf, sizeof(buf), "Grade:");
	evt = new DispDrawTextReq(ILI9341, GET_HSMN(), buf, 120, 170,
	COLOR24_WHITE, COLOR24_BLACK, 2);
	Fw::Post(evt);

	// Display Grade Capacity
	snprintf(buf, sizeof(buf), "%.1f", me->m_currGrade->GetGradeCapacity());
	evt = new DispDrawTextReq(ILI9341, GET_HSMN(), buf, 120, 200,
	COLOR24_WHITE, COLOR24_BLACK, 2);
	Fw::Post(evt);

	// User Notification Drawing
	evt = new DispDrawRectReq(ILI9341, GET_HSMN(), 0, 120, 160, 20,
	COLOR24_BLACK);
	Fw::Post(evt);

	// Print Message to LCD
	snprintf(buf, sizeof(buf), me->m_toUser);
	evt = new DispDrawTextReq(ILI9341, GET_HSMN(), buf, 0, 120,
	COLOR24_RED, COLOR24_BLACK, 2);
	Fw::Post(evt);

	evt = new DispDrawEndReq(ILI9341, GET_HSMN(), GEN_SEQ());

	Fw::Post(evt);

}

void FuelPump::GradeDraw(FuelPump *const me, uint32_t grade, char *buf,
		int16_t rect_x, int16_t text_x) {

	Evt *evt = new DispDrawRectReq(ILI9341, GET_HSMN(), rect_x, 250, 50, 50,
	COLOR24_GREEN);
	Fw::Post(evt);

	// Print Selected Grade
	snprintf(buf, sizeof(buf), "%d", grade);
	evt = new DispDrawTextReq(ILI9341, GET_HSMN(), buf, text_x, 265,
	COLOR24_WHITE, COLOR24_GREEN, 3);
	Fw::Post(evt);

}

void FuelPump::ValuesReset(FuelPump *const me)
{
	// --- Immediate Reset ----

	// reset price
	me->m_price = 0;
	// reset gallons
	me->m_gallons = 0;
	// reset price rate
	me->m_price_rate = 0;
	// reset gallon rate
	me->m_gallon_rate = 0;

	me->m_paid = false;

	me->m_ingore = false;
	me->m_currGrade = NULL;
}


}
