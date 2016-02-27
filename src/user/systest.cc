/******************************************************************************
    Copyright © 2012-2015 Martin Karsten

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/
//#include "syscalls.h"
#include "kernel/Clock.h"
#include "runtime/globalVar.h"
#include <stdio.h>
//#include "kernel/Output.h"
volatile mword Clock::tick;

int main() {
    mword start_time = CPU::readTSC();
    Clock::wait(1024);
    mword end_time = CPU::readTSC();
    mword result2 = end_time - start_time;
    cout<<result2;
    Clock::wait(10240);
    resultVar = result2;	
};
