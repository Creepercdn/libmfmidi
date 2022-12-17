/*
 * This file is a part of libmfmidi.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

/// \file SimpleTrackPlayer.hpp
/// \author Creepercdn (creepercdn@outlook.com)
/// \brief A very simple multithreaded MIDITrack player

#pragma once

#include "libmfmidi/mfconcepts.hpp"
#include "libmfmidi/miditrack.hpp"
#include <functional>
#include "libmfmidi/abstractmididevice.hpp"
#include "libmfmidi/abstracttimer.hpp"

// Together for a std::shared_future!

namespace libmfmidi {
    /// \deprecated This class is deprecated because it is unstable and buggy.
    ///
    class [[deprecated]] SimpleTrackPlayer {
    public:
        static constexpr unsigned int PLAYER_TICKDELAY = 1; // 1ms
        void play()
        {
            mtimer->start(PLAYER_TICKDELAY);
            mplay = true;
        }

        void pause()
        {
            if(mtimer != nullptr){
                mtimer->stop();
            }
            mplay = false;
        }

        [[nodiscard]] MIDIClockTime tickTime() const
        {
            return mabstimertick;
        }

        void goZero()
        {
            pause();
            cur      = mtrk.begin();
            mreltimertick = 0;
            mabstimertick = 0;
        }

        void setTimer(AbstractTimer* timer)
        {
            pause();
            mtimer = timer;
            timer->setCallback([this] { _timertick(); });
        }

        bool isPlaying() const noexcept
        {
            return mplay;
        }

        void setMsgProcessor(MIDIProcessorFunction processor) noexcept
        {
            mprocess = std::move(processor);
        }

        void setDivision(MIDIDivision divi)
        {
            division = divi;
            updateTickTime();
        }

        void setTrack(MIDITrack&& trk)
        {
            mtrk = std::move(trk);
            // pre processing
            auto it = mtrk.begin();
            while (it != mtrk.end()) {
                if (!mprocess(*it)) {
                    auto tmp = it - mtrk.begin();
                    if (mtrk.end() - it > 1) {
                        (it + 1)->setDeltaTime(it->deltaTime() + (it + 1)->deltaTime());
                    }
                    mtrk.erase(it);
                    it = mtrk.begin() + tmp;
                    continue;
                }
                ++it;
            }
            cur = mtrk.begin();
        }

        void setDriver(AbstractMIDIDevice* adrv)
        {
            pause();
            if(adrv->outputAvailable()){
                drv = adrv;
            }
        }

        void setTempo(MIDITempo bpm)
        {
            tempobpm = bpm;
            updateTickTime();
        }

    private:
        AbstractMIDIDevice*   drv;
        AbstractTimer*        mtimer{};
        MIDITrack             mtrk;
        MIDITrack::iterator   cur;
        MIDIClockTime         mreltimertick = 0;
        bool                  mplay    = false;
        MIDITempo              tempobpm = MIDITempo::fromBPM(120);
        MIDIDivision          division{96};
        MIDIProcessorFunction  mprocess;
        MIDIClockTime mabstimertick = 0;
        double                 tickDelay = divisionToSec(division, tempobpm) * 1000; // cache: milliseconds per midi tick

        void updateTickTime() noexcept
        {
            tickDelay = divisionToSec(division, tempobpm)*1000;
        }

        void _timertick()
        {
            // About this loop:
            // For repeated 0 or low deltatime events
            while(true){
                if (cur >= mtrk.end()) {
                    pause();
                    return;
                }
                if (mreltimertick * PLAYER_TICKDELAY <= tickDelay * cur->deltaTime()) {
                    ++mreltimertick;
                    return;
                }
                mabstimertick += cur->deltaTime();
                if (cur->MFMarker() == MFMessageMark::Tempo) {
                    setTempo(MIDITempo::fromMSPQ(rawCat(cur->byte0(), cur->byte1(), cur->byte2(), cur->byte3())));
                } else {
                    drv->sendMsg(*cur);
                }
                ++cur;
                mreltimertick = 0; // when next tick come it past 1 tick, but there is a loop so no time between next tick
            }
        }
    };
}
