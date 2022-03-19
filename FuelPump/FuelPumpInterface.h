/*
 * FuelPumpInterface.h
 *
 *  Created on: Jun 14, 2021
 *      Author: EdwinKaburu
 */

#ifndef FUEL_PUMP_INTERFACE_H
#define FUEL_PUMP_INTERFACE_H

#include "fw_def.h"
#include "fw_evt.h"
#include "app_hsmn.h"

using namespace QP;
using namespace FW;

namespace APP {

#define FUEL_PUMP_INTERFACE_EVT \
    ADD_EVT(FUEL_PUMP_START_REQ) \
    ADD_EVT(FUEL_PUMP_START_CFM) \
    ADD_EVT(FUEL_PUMP_STOP_REQ) \
    ADD_EVT(FUEL_PUMP_STOP_CFM) \
    ADD_EVT(FUEL_PUMP_PAYMENT_IND) \
    ADD_EVT(FUEL_PUMP_GRADE_REQ) \
    ADD_EVT(FUEL_PUMP_GRADE_CFM) \
	ADD_EVT(FUEL_PUMP_FILL_IND) \
	ADD_EVT(FUEL_PUMP_CPRICERATE_REQ)\
	ADD_EVT(FUEL_PUMP_CPRICERATE_CFM) \
	ADD_EVT(FUEL_PUMP_CGALLONRATE_IND)

#undef ADD_EVT
#define ADD_EVT(e_) e_,

enum {
	FUEL_PUMP_INTERFACE_EVT_START = INTERFACE_EVT_START(FUEL_PUMP),
	FUEL_PUMP_INTERFACE_EVT
};

enum {
	FUEL_PUMP_REASON_UNSPEC = 0,
	FUEL_PUMP_REASON_INVALID_GRADE,
};

class FuelPumpStartReq: public Evt {
public:
	enum {
		TIMEOUT_MS = 100
	};

	FuelPumpStartReq(Hsmn to, Hsmn from, Sequence seq) :
			Evt(FUEL_PUMP_START_REQ, to, from, seq) {
	}
};

class FuelPumpStartCfm: public ErrorEvt {
public:
	FuelPumpStartCfm(Hsmn to, Hsmn from, Sequence seq, Error error,
			Hsmn origin = HSM_UNDEF, Reason reason = 0) :
			ErrorEvt(FUEL_PUMP_START_CFM, to, from, seq, error, origin, reason) {
	}
};

class FuelPumpStopReq: public Evt {
public:
	enum {
		TIMEOUT_MS = 200
	};
	FuelPumpStopReq(Hsmn to, Hsmn from, Sequence seq) :
			Evt(FUEL_PUMP_STOP_REQ, to, from, seq) {
	}

};

class FuelPumpStopCfm: public ErrorEvt {
public:
	FuelPumpStopCfm(Hsmn to, Hsmn from, Sequence seq, Error error, Hsmn origin =
			HSM_UNDEF, Reason reason = 0) :
			ErrorEvt(FUEL_PUMP_STOP_CFM, to, from, seq, error, origin, reason) {
	}
};

class FuelPumpPaymentInd: public Evt {
public:
	typedef enum {
		CASH, CREDIT
	} PaymentType;

	FuelPumpPaymentInd(Hsmn to, Hsmn from, PaymentType Type = CASH,
			uint8_t amount = 20) :
			Evt(FUEL_PUMP_PAYMENT_IND, to, from), m_paymentype(Type), m_amount(
					amount) {
	}

	PaymentType GetPayment() const {
		return m_paymentype;
	}
	uint8_t GetAmount() const {
		return m_amount;
	}

private:
	PaymentType m_paymentype;
	uint8_t m_amount;
};

class FuelPumpGradeReq: public Evt {
public:
	enum {
		TIMEOUT_MS = 100,
	};

	FuelPumpGradeReq(Hsmn to, Hsmn from, Sequence seq, uint32_t type = 87) :
			Evt(FUEL_PUMP_GRADE_REQ, to, from, seq), m_gradeIndex(type) {
	}
	uint32_t GetGradeType() const { return m_gradeIndex; }

private:
	uint32_t m_gradeIndex;

};

class FuelPumpGradeCfm: public ErrorEvt {
public:
	FuelPumpGradeCfm(Hsmn to, Hsmn from, Sequence seq, Error error,
			Hsmn origin = HSM_UNDEF, Reason reason = 0) :
			ErrorEvt(FUEL_PUMP_GRADE_CFM, to, from, seq, error, origin, reason) {
	}
};

class FuelPumpFillInd: public Evt {
public:
	FuelPumpFillInd(Hsmn to, Hsmn from, Sequence seq = 0) :
			Evt(FUEL_PUMP_FILL_IND, to, from, seq) {
	}
};

class FuelPumpCPriceRateReq: public Evt{
public:
	enum{
		TIMEOUT_MS = 100,
	};
	FuelPumpCPriceRateReq(Hsmn to, Hsmn from, Sequence seq, uint32_t type = 87, float p_rate = 0.25):
		Evt(FUEL_PUMP_CPRICERATE_REQ, to , from, seq), m_gradeIndex(type), m_price_rate(p_rate){
	}

	uint32_t GetGradeType() const { return m_gradeIndex; }

	float GetPriceRate() const { return m_price_rate; }

private:
	float m_price_rate;
	uint32_t m_gradeIndex;

};

class FuelPumpCPriceRateCfm : public ErrorEvt {
public:
	FuelPumpCPriceRateCfm(Hsmn to, Hsmn from, Sequence seq, Error error, Hsmn origin = HSM_UNDEF, Reason reason = 0) :
        ErrorEvt(FUEL_PUMP_CPRICERATE_CFM, to, from, seq, error, origin, reason) {}
};

class FuelPumpCGallonRateInd: public Evt{
public:

	FuelPumpCGallonRateInd(Hsmn to, Hsmn from, float  g_rate =  0.10) :
				Evt(FUEL_PUMP_CGALLONRATE_IND, to, from), m_gallon_rate(g_rate){
		}

	float GetGallonRate() const {
		return m_gallon_rate;
	}
private:
	float m_gallon_rate;
};




} // namespace APP

#endif /* FUEL_PUMP_INTERFACE_H */
