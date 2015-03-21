/*
 * Copyright (C) 2014 Dominic Ward <contactdominicward@gmail.com>
 *
 * This file is part of Loudness
 *
 * Loudness is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Loudness is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Loudness.  If not, see <http://www.gnu.org/licenses/>. 
 */

#include "IntegratedLoudnessGM.h"
#include "../Support/AuditoryTools.h"

namespace loudness{

    IntegratedLoudnessGM::IntegratedLoudnessGM(const string& author, Real cParam) :
        Module("IntegratedLoudnessGM"),
        cParam_(cParam)
    {
        configureSmoothingTimes(author);
        LOUDNESS_DEBUG(name_ << ": Constructed.");
    }

    void IntegratedLoudnessGM::configureSmoothingTimes(const string& author)
    {
        if (author == "GM2002")
        {
            setAttackTimeSTL(-0.001/log(1-0.045));    
            setReleaseTimeSTL(-0.001/log(1-0.02));
            setAttackTimeLTL(-0.001/log(1-0.01));
            setReleaseTimeLTL(-0.001/log(1-0.0005));
        }
        else if (author == "GM2003")
        {
            setAttackTimeSTL(-0.001/log(1-0.045));    
            setReleaseTimeSTL(-0.001/log(1-0.02));
            setAttackTimeLTL(-0.001/log(1-0.01));
            setReleaseTimeLTL(-0.001/log(1-0.005));
        }
        else if (author == "CH2012")
        {
            setAttackTimeSTL(0.016);    
            setReleaseTimeSTL(0.032);
            setAttackTimeLTL(0.1);
            setReleaseTimeLTL(2.0);
        }
        else
        {
            LOUDNESS_WARNING(name_ << ": Using smoothing times given by Glasberg and Moore (2002).");
            configureSmoothingTimes("GM2002");
        }
    }

    IntegratedLoudnessGM::~IntegratedLoudnessGM()
    {
    };

    void IntegratedLoudnessGM::setAttackTimeSTL(Real attackTimeSTL)
    {
        attackTimeSTL_ = attackTimeSTL;
    }

    void IntegratedLoudnessGM::setReleaseTimeSTL(Real releaseTimeSTL)
    {
        releaseTimeSTL_ = releaseTimeSTL;
    }

    void IntegratedLoudnessGM::setAttackTimeLTL(Real attackTimeLTL)
    {
        attackTimeLTL_ = attackTimeLTL;
    }

    void IntegratedLoudnessGM::setReleaseTimeLTL(Real releaseTimeLTL)
    {
        releaseTimeLTL_ = releaseTimeLTL;
    }

    bool IntegratedLoudnessGM::initializeInternal(const SignalBank &input)
    {
        LOUDNESS_ASSERT(input.getNChannels() > 1,
                name_ << ": Insufficient number of input channels.");
        LOUDNESS_ASSERT(input.getNEars() > 2,
                name_ << ": A human has no more than two ears");

        //assumes uniformly spaced ERB filters
        camStep_ = freqToCam(input.getCentreFreq(1)) - freqToCam(input.getCentreFreq(0));
        LOUDNESS_DEBUG(name_ << ": Filter spacing (Cams): " << camStep_);

        //if diotic presentation, multiply total loudness by 2
        if (input.getNEars() == 1)
        {
            cParam_ *= 2;
            LOUDNESS_DEBUG(name_ << ": Diotic presentation, loudness will be multiplied by 2.");
        }

        //update scaling factor
        cParam_ *= camStep_;

        //coefficient configuration
        timeStep_ = 1.0/input.getFrameRate();
        LOUDNESS_DEBUG(name_ << ": Time step: " << timeStep_);
        
        //short-term loudness time-constants (alpha from paper)
        attackSTLCoef_ = 1 - exp(-timeStep_ / attackTimeSTL_);
        releaseSTLCoef_ = 1 - exp(-timeStep_ / releaseTimeSTL_);
        attackLTLCoef_ = 1 - exp(-timeStep_ / attackTimeLTL_);
        releaseLTLCoef_ = 1 - exp(-timeStep_ / releaseTimeLTL_);

        //output SignalBank
        output_.initialize(input.getNEars(), 3, 1, input.getFs());
        output_.setFrameRate(input.getFrameRate());

        return 1;
    }

    void IntegratedLoudnessGM::processInternal(const SignalBank &input)
    {       
        for (int ear = 0; ear < input.getNEars(); ear++)
        {
            const Real* inputSpecificLoudness = input.getSingleSampleReadPointer(ear, 0);
            Real* outputIntegratedLoudness = output_.getSingleSampleWritePointer(ear, 0);

            //instantaneous loudness
            Real il = 0.0;
            for (int chn = 0; chn < input.getNChannels(); chn++)
                il += inputSpecificLoudness[chn];

            //apply scaling factor
            il *= cParam_;

            //short-term loudness
            Real prevSTL = outputIntegratedLoudness[1];
            Real stl = 0.0;

            if (il > prevSTL)
                stl = attackSTLCoef_ * (il - prevSTL) + prevSTL;
            else
                stl = releaseSTLCoef_ * (il - prevSTL) + prevSTL;

            //long-term loudness
            Real prevLTL = outputIntegratedLoudness[2];
            Real ltl = 0.0;

            if (stl > prevLTL)
                ltl = attackLTLCoef_ * (stl - prevLTL) + prevLTL;
            else
                ltl = releaseLTLCoef_ * (stl - prevLTL) + prevLTL;
            
            //fill output SignalBank
            outputIntegratedLoudness[0] = il;
            outputIntegratedLoudness[1] = stl;
            outputIntegratedLoudness[2] = ltl;
        }
    }

    //output SignalBanks are cleared so not to worry about filter state
    void IntegratedLoudnessGM::resetInternal()
    {
    }
}

