/*
 * Copyright 2018 Google, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Authors: Gabe Black
 */

#include "base/logging.hh"
#include "base/types.hh"
#include "sim/core.hh"
#include "sim/eventq.hh"
#include "systemc/core/kernel.hh"
#include "systemc/core/sched_event.hh"
#include "systemc/core/scheduler.hh"
#include "systemc/ext/channel/sc_clock.hh"
#include "systemc/ext/core/sc_module.hh" // for sc_gen_unique_name

namespace sc_gem5
{

class ClockTick : public ScEvent
{
  private:
    ::sc_core::sc_clock *clock;
    ::sc_core::sc_time _period;
    std::string _name;
    Process *p;
    ProcessMemberFuncWrapper<::sc_core::sc_clock> funcWrapper;
    std::string _procName;

  public:
    ClockTick(::sc_core::sc_clock *clock, bool to,
            ::sc_core::sc_time _period) :
        ScEvent([this]() { tick(); }),
        clock(clock), _period(_period), _name(clock->name()),
        funcWrapper(clock, to ? &::sc_core::sc_clock::tickUp :
                                &::sc_core::sc_clock::tickDown)
    {
        _name += (to ? ".up_tick" : ".down_tick");
        _procName = _name + ".p";
        p = newMethodProcess(_procName.c_str(), &funcWrapper);
        scheduler.dontInitialize(p);
    }

    ~ClockTick()
    {
        if (scheduled())
            scheduler.deschedule(this);
        p->popListNode();
    }

    void
    tick()
    {
        scheduler.schedule(this, _period);
        p->ready();
    }
};

};

namespace sc_core
{

sc_clock::sc_clock() :
    sc_clock(sc_gen_unique_name("clock"), sc_time(1.0, SC_NS),
            0.5, SC_ZERO_TIME, true)
{}

sc_clock::sc_clock(const char *name) :
    sc_clock(name, sc_time(1.0, SC_NS), 0.5, SC_ZERO_TIME, true)
{}

sc_clock::sc_clock(const char *name, const sc_time &period,
                   double duty_cycle, const sc_time &start_time,
                   bool posedge_first) :
    sc_interface(), sc_signal<bool>(name, posedge_first ? false : true),
    _period(period), _dutyCycle(duty_cycle), _startTime(start_time),
    _posedgeFirst(posedge_first)
{
    _gem5UpEdge = new ::sc_gem5::ClockTick(this, true, period);
    _gem5DownEdge = new ::sc_gem5::ClockTick(this, false, period);
}

sc_clock::sc_clock(const char *name, double period_v, sc_time_unit period_tu,
                   double duty_cycle) :
    sc_clock(name, sc_time(period_v, period_tu), duty_cycle, SC_ZERO_TIME,
            true)
{}

sc_clock::sc_clock(const char *name, double period_v, sc_time_unit period_tu,
                   double duty_cycle, double start_time_v,
                   sc_time_unit start_time_tu, bool posedge_first) :
    sc_clock(name, sc_time(period_v, period_tu), duty_cycle,
            sc_time(start_time_v, start_time_tu), posedge_first)
{}

sc_clock::sc_clock(const char *name, double period, double duty_cycle,
                   double start_time, bool posedge_first) :
    sc_clock(name, sc_time(period, true), duty_cycle,
            sc_time(start_time, true), posedge_first)
{}

sc_clock::~sc_clock()
{
    if (_gem5UpEdge->scheduled())
        ::sc_gem5::scheduler.deschedule(_gem5UpEdge);
    if (_gem5DownEdge->scheduled())
        ::sc_gem5::scheduler.deschedule(_gem5DownEdge);
    delete _gem5UpEdge;
    delete _gem5DownEdge;
}

void
sc_clock::write(const bool &)
{
    panic("write() called on sc_clock.");
}

const sc_time &sc_clock::period() const { return _period; }
double sc_clock::duty_cycle() const { return _dutyCycle; }
const sc_time &sc_clock::start_time() const { return _startTime; }
bool sc_clock::posedge_first() const { return _posedgeFirst; }

const sc_time &
sc_clock::time_stamp()
{
    warn("%s not implemented.\n", __PRETTY_FUNCTION__);
    return *(const sc_time *)nullptr;
}

void
sc_clock::before_end_of_elaboration()
{
    if (_posedgeFirst) {
        ::sc_gem5::scheduler.schedule(_gem5UpEdge, _startTime);
        ::sc_gem5::scheduler.schedule(_gem5DownEdge,
                _startTime + _period * _dutyCycle);
    } else {
        ::sc_gem5::scheduler.schedule(_gem5DownEdge, _startTime);
        ::sc_gem5::scheduler.schedule(_gem5UpEdge,
                _startTime + _period * (1.0 - _dutyCycle));
    }
}

} // namespace sc_core
