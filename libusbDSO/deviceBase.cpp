////////////////////////////////////////////////////////////////////////////////
//
//  OpenHantek
//
//  Copyright (C) 2008, 2009  Oleg Khudyakov
//  prcoder@potrebitel.ru
//  Copyright (C) 2010 - 2012  Oliver Haag
//  oliver.haag@gmail.com
//
//  This program is free software: you can redistribute it and/or modify it
//  under the terms of the GNU General Public License as published by the Free
//  Software Foundation, either version 3 of the License, or (at your option)
//  any later version.
//
//  This program is distributed in the hope that it will be useful, but WITHOUT
//  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
//  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
//  more details.
//
//  You should have received a copy of the GNU General Public License along with
//  this program.  If not, see <http://www.gnu.org/licenses/>.
//
////////////////////////////////////////////////////////////////////////////////

#include "deviceBase.h"
#include <QDebug>

namespace DSO {
    void DeviceBase::resetSettings()
    {
        _samples.resize(_specification.channels);
        _settings.samplerate.limits = &(_specification.samplerate_single);
        _specification.gainLevel.clear();

        for(DSO::dsoSettingsChannel& c: _settings.voltage)
            c = DSO::dsoSettingsChannel();

        for(double& c: _settings.trigger.level)
            c = 0.0;

        // clear
        _specification.samplerate_single.recordTypes.clear();
        _specification.samplerate_multi.recordTypes.clear();

        _specification.specialTriggerSources.clear();

    }

    /// \brief Get a list of the names of the special trigger sources.
    const std::vector<std::string>& DeviceBase::getSpecialTriggerSources() const {
        return _specification.specialTriggerSources;
    }


    ErrorCode DeviceBase::setChannelUsed(unsigned int channel, bool used)
    {
        if(channel >= _specification.channels)
            return ErrorCode::ERROR_PARAMETER;

        // Update _settings
        _settings.voltage[channel].used = used;
        unsigned int channelCount = 0;
        for(unsigned channelCounter = 0; channelCounter < _specification.channels; ++channelCounter) {
            if(_settings.voltage[channelCounter].used)
                ++channelCount;
        }

        // Check if fast rate mode availability changed
        bool fastRateChanged = (_settings.usedChannels <= 1) != (channelCount <= 1);
        _settings.usedChannels = channelCount;

        if(fastRateChanged)
            notifySamplerateLimitsChanged();

        return ErrorCode::ERROR_PARAMETER;
    }

    ErrorCode DeviceBase::setCoupling(unsigned int channel, Coupling coupling)
    {
        _settings.voltage[channel].coupling = coupling;
        return ErrorCode::ERROR_PARAMETER;
    }

    ErrorCode DeviceBase::setGain(unsigned int channel, double gain)
    {
        if(!isDeviceConnected())
            return ErrorCode::ERROR_CONNECTION;

        if(channel >= _specification.channels)
            return ErrorCode::ERROR_PARAMETER;

        // Find lowest gain voltage thats at least as high as the requested
        unsigned gainID;
        for(gainID = 0; gainID < _specification.gainLevel.size() - 1; ++gainID)
            if(_specification.gainLevel[gainID].gainSteps >= gain)
                 break;

        // No gain level found
        if(gainID == _specification.gainLevel.size())
            return ErrorCode::ERROR_PARAMETER;

        updateGain(channel, _specification.gainLevel[gainID].gainIndex, gainID);
        _settings.voltage[channel].gainID = gainID;
        setOffset(channel, _settings.voltage[channel].offset);

        // _specification.gainSteps[gainId]
        return ErrorCode::ERROR_NONE;
    }

    ErrorCode DeviceBase::setOffset(unsigned int channel, double offset)
    {
        if(!isDeviceConnected())
            return ErrorCode::ERROR_CONNECTION;

        if(channel >= _specification.channels)
            return ErrorCode::ERROR_PARAMETER;

        // Calculate the offset value
        // The range is given by the calibration data (convert from big endian)
        unsigned int gainID = _settings.voltage[channel].gainID;
        unsigned short int minimum, maximum, offsetDiff, offsetValue;
        minimum = _specification.gainLevel[gainID].offset[channel].minimum;
        maximum = _specification.gainLevel[gainID].offset[channel].maximum;
        offsetDiff = maximum - minimum;
        offsetValue = offset * offsetDiff + minimum + 0.5;
        double offsetReal = (double) offset + 0.5 / offsetDiff;

        updateOffset(channel, offsetValue);

        _settings.voltage[channel].offset = offset;
        _settings.voltage[channel].offsetReal = offsetReal;

        updateTriggerLevel(channel, _settings.trigger.level[channel]);

        // offsetReal;
        return ErrorCode::ERROR_NONE;
    }

    ErrorCode DeviceBase::setTriggerSource(bool special, unsigned int channel)
    {
        ErrorCode c = updateTriggerSource(special, channel);
        if (c != ErrorCode::ERROR_NONE) return c;

        _settings.trigger.special = special;
        _settings.trigger.source = channel;

        if(!special)
            this->setTriggerLevel(channel, _settings.trigger.level[channel]);

        return ErrorCode::ERROR_NONE;
    }

    ErrorCode DeviceBase::setTriggerLevel(unsigned int channel, double level)
    {
        ErrorCode c = updateTriggerLevel(channel, level);
        if (c != ErrorCode::ERROR_NONE) return c;

        _settings.trigger.level[channel] = level;
        return ErrorCode::ERROR_NONE;
    }

    ErrorCode DeviceBase::setTriggerSlope(Slope slope)
    {
        ErrorCode c = updateTriggerSlope(slope);
        if (c != ErrorCode::ERROR_NONE) return c;

        _settings.trigger.slope = slope;
        return ErrorCode::ERROR_NONE;
    }

    ErrorCode DeviceBase::setTriggerMode(TriggerMode mode) {
        if(!isDeviceConnected())
            return ErrorCode::ERROR_CONNECTION;

        if(mode < TriggerMode::AUTO || mode > TriggerMode::SINGLE)
            return ErrorCode::ERROR_PARAMETER;

        _settings.trigger.mode = mode;
        return ErrorCode::ERROR_NONE;
    }
}
