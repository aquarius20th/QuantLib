
/*
 Copyright (C) 2004 StatPro Italia srl

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it under the
 terms of the QuantLib license.  You should have received a copy of the
 license along with this program; if not, please email quantlib-dev@lists.sf.net
 The license is also available online at http://quantlib.org/html/license.html

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

#include "dividendeuropeanoption.hpp"
#include "utilities.hpp"
#include <ql/DayCounters/actual360.hpp>
#include <ql/Instruments/dividendvanillaoption.hpp>
#include <ql/PricingEngines/Vanilla/analyticdividendeuropeanengine.hpp>
#include <ql/TermStructures/flatforward.hpp>
#include <ql/Volatilities/blackconstantvol.hpp>
#include <map>

using namespace QuantLib;
using namespace boost::unit_test_framework;

#define REPORT_FAILURE(greekName, payoff, exercise, s, q, r, today, \
                       v, expected, calculated, error, tolerance) \
    BOOST_FAIL(exerciseTypeToString(exercise) + " " \
               + OptionTypeFormatter::toString(payoff->optionType()) + \
               " option with " \
               + payoffTypeToString(payoff) + " payoff:\n" \
               "    spot value: " \
               + DecimalFormatter::toString(s) + "\n" \
               "    strike:           " \
               + DecimalFormatter::toString(payoff->strike()) +"\n" \
               "    dividend yield:   " \
               + RateFormatter::toString(q) + "\n" \
               "    risk-free rate:   " \
               + RateFormatter::toString(r) + "\n" \
               "    reference date:   " \
               + DateFormatter::toString(today) + "\n" \
               "    maturity:         " \
               + DateFormatter::toString(exercise->lastDate()) + "\n" \
               "    volatility:       " \
               + VolatilityFormatter::toString(v) + "\n\n" \
               "    expected   " + greekName + ": " \
               + DecimalFormatter::toString(expected) + "\n" \
               "    calculated " + greekName + ": " \
               + DecimalFormatter::toString(calculated) + "\n" \
               "    error:            " \
               + DecimalFormatter::toString(error) + "\n" \
               "    tolerance:        " \
               + DecimalFormatter::toString(tolerance));

// tests

namespace {

    void teardown() {
        Settings::instance().setEvaluationDate(Date());
    }

}

void DividendEuropeanOptionTest::testGreeks() {

    BOOST_MESSAGE("Testing dividend European option greeks...");

    QL_TEST_BEGIN

    std::map<std::string,Real> calculated, expected, tolerance;
    tolerance["delta"] = 1.0e-5;
    tolerance["gamma"] = 1.0e-5;
    tolerance["theta"] = 1.0e-5;
    tolerance["rho"]   = 1.0e-5;
    tolerance["vega"]  = 1.0e-5;

    Option::Type types[] = { Option::Call, Option::Put };
    Real strikes[] = { 50.0, 99.5, 100.0, 100.5, 150.0 };
    Real underlyings[] = { 100.0 };
    Rate qRates[] = { 0.00, 0.10, 0.30 };
    Rate rRates[] = { 0.01, 0.05, 0.15 };
    Integer lengths[] = { 1, 2 };
    Volatility vols[] = { 0.05, 0.20, 0.70 };

    DayCounter dc = Settings::instance().dayCounter();
    Date today = Date::todaysDate();
    Settings::instance().setEvaluationDate(today);

    boost::shared_ptr<SimpleQuote> spot(new SimpleQuote(0.0));
    boost::shared_ptr<SimpleQuote> qRate(new SimpleQuote(0.0));
    Handle<YieldTermStructure> qTS(flatRate(qRate, dc));
    boost::shared_ptr<SimpleQuote> rRate(new SimpleQuote(0.0));
    Handle<YieldTermStructure> rTS(flatRate(rRate, dc));
    boost::shared_ptr<SimpleQuote> vol(new SimpleQuote(0.0));
    Handle<BlackVolTermStructure> volTS(flatVol(vol));

    for (Size i=0; i<LENGTH(types); i++) {
      for (Size j=0; j<LENGTH(strikes); j++) {
        for (Size k=0; k<LENGTH(lengths); k++) {
          Date exDate = today +
            lengths[k]*Settings::instance().dayCounterBase();
          boost::shared_ptr<Exercise> exercise(new EuropeanExercise(exDate));

          std::vector<Date> dividendDates;
          std::vector<Real> dividends;
          for (Date d = today + 3*Months;
                    d < exercise->lastDate();
                    d += 6*Months) {
              dividendDates.push_back(d);
              dividends.push_back(5.0);
          }

          boost::shared_ptr<StrikedTypePayoff> payoff(
                                new PlainVanillaPayoff(types[i], strikes[j]));

          boost::shared_ptr<PricingEngine> engine(
                                          new AnalyticDividendEuropeanEngine);

          boost::shared_ptr<BlackScholesProcess> stochProcess(
              new BlackScholesProcess(Handle<Quote>(spot), qTS, rTS, volTS));

          DividendVanillaOption option(stochProcess, payoff, exercise,
                                       dividendDates, dividends, engine);

          for (Size l=0; l<LENGTH(underlyings); l++) {
            for (Size m=0; m<LENGTH(qRates); m++) {
              for (Size n=0; n<LENGTH(rRates); n++) {
                for (Size p=0; p<LENGTH(vols); p++) {
                    Real u = underlyings[l];
                    Rate q = qRates[m],
                         r = rRates[n];
                    Volatility v = vols[p];
                    spot->setValue(u);
                    qRate->setValue(q);
                    rRate->setValue(r);
                    vol->setValue(v);

                    Real value = option.NPV();
                    calculated["delta"]  = option.delta();
                    calculated["gamma"]  = option.gamma();
                    calculated["theta"]  = option.theta();
                    calculated["rho"]    = option.rho();
                    calculated["vega"]   = option.vega();

                    if (value > spot->value()*1.0e-5) {
                        // perturb spot and get delta and gamma
                        Real du = u*1.0e-4;
                        spot->setValue(u+du);
                        Real value_p = option.NPV(),
                             delta_p = option.delta();
                        spot->setValue(u-du);
                        Real value_m = option.NPV(),
                             delta_m = option.delta();
                        spot->setValue(u);
                        expected["delta"] = (value_p - value_m)/(2*du);
                        expected["gamma"] = (delta_p - delta_m)/(2*du);

                        // perturb risk-free rate and get rho
                        Spread dr = r*1.0e-4;
                        rRate->setValue(r+dr);
                        value_p = option.NPV();
                        rRate->setValue(r-dr);
                        value_m = option.NPV();
                        rRate->setValue(r);
                        expected["rho"] = (value_p - value_m)/(2*dr);

                        // perturb volatility and get vega
                        Spread dv = v*1.0e-4;
                        vol->setValue(v+dv);
                        value_p = option.NPV();
                        vol->setValue(v-dv);
                        value_m = option.NPV();
                        vol->setValue(v);
                        expected["vega"] = (value_p - value_m)/(2*dv);

                        // perturb date and get theta
                        Time dT = dc.yearFraction(today-1, today+1);
                        Settings::instance().setEvaluationDate(today-1);
                        value_m = option.NPV();
                        Settings::instance().setEvaluationDate(today+1);
                        value_p = option.NPV();
                        Settings::instance().setEvaluationDate(today);
                        expected["theta"] = (value_p - value_m)/dT;

                        // compare
                        std::map<std::string,Real>::iterator it;
                        for (it = calculated.begin();
                             it != calculated.end(); ++it) {
                            std::string greek = it->first;
                            Real expct = expected  [greek],
                                 calcl = calculated[greek],
                                 tol   = tolerance [greek];
                            Real error = relativeError(expct,calcl,u);
                            if (error>tol) {
                                REPORT_FAILURE(greek, payoff, exercise,
                                               u, q, r, today, v,
                                               expct, calcl, error, tol);
                            }
                        }
                    }
                }
              }
            }
          }
        }
      }
    }

    QL_TEST_TEARDOWN
}


test_suite* DividendEuropeanOptionTest::suite() {
    test_suite* suite = BOOST_TEST_SUITE("Dividend European option tests");
    suite->add(BOOST_TEST_CASE(&DividendEuropeanOptionTest::testGreeks));
    return suite;
}

