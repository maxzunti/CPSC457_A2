/******************************************************************************
    Copyright � 2012-2015 Martin Karsten

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
#include "runtime/Thread.h"
#include "kernel/AddressSpace.h"
#include "kernel/Clock.h"
#include "kernel/Output.h"
#include "world/Access.h"
#include "machine/Machine.h"
#include "devices/Keyboard.h"
#include "devices/RTC.h"
#include "main/UserMain.h"
#include "generic/tree.h"




AddressSpace kernelSpace(true); // AddressSpace.h
volatile mword Clock::tick;     // Clock.h

extern Keyboard keyboard;

#if TESTING_KEYCODE_LOOP
static void keybLoop() {
  for (;;) {
    Keyboard::KeyCode c = keyboard.read();
    StdErr.print(' ', FmtHex(c));
  }
}
#endif

void kosMain() {
  KOUT::outl("Welcome to KOS!", kendl);
  auto iter = kernelFS.find("motb");
  if (iter == kernelFS.end()) {
    KOUT::outl("motb information not found");
  } else {
    FileAccess f(iter->second);
    for (;;) {
      char c;
      if (f.read(&c, 1) == 0) break;
      KOUT::out1(c);
    }
    KOUT::outl();
  }
  auto iter2 = kernelFS.find("schedparam");
  if (iter2 == kernelFS.end()) {
    KOUT::outl("schedparam information not found");
  } else {
    FileAccess f(iter2->second);
    const char * smgName = "schedMinGranularity = ";
    unsigned int x = 0;
    for (;;) {
      char c;
      if (f.read(&c, 1) == 0) break;
      if (c == smgName[x]) {
        x++;
        if (x == strlen(smgName)) // If found, read from =
            // After this point, get next num
            KOUT::outl("Found string!");
      } else {
        x = 0;
      }

    }
    KOUT::outl();
  }

  Tree<int> readyTree;

//Test Case 1: Fetch from empty tree
  readyTree.find(12);

//Test Case 2: Insert into tree (empty tree)
  readyTree.insert(12);
  readyTree.insert(10);
  int* k = readyTree.readMinNode();
  KOUT::outl(*k);
//Test Case 3: Deletion
  readyTree.deleteNode(12);

//Test Case 4: Trying to delete when the tree does not exist
  readyTree.deleteNode(10);

//Test Case 5: Inorder traversal (TODO)

//Test Case 6: Preorder traversal (TODO)

//Test Case 7: Post Order traversal (TODO)

//Test Case 8: Memory allocation fails  <kASSERT>

  Clock::wait(1024);
 




  mword start_time = CPU::readTSC();
  KOUT::outl(start_time);
  Clock::wait(1024);
  mword end_time = CPU::readTSC();
  KOUT::outl(end_time);
  mword result = end_time - start_time;
  KOUT::outl(result);

  //uint8_t a = CPU::in8(0x71);
 // int b = static_cast<int>(a) ;
 // KOUT::outl(b);
  //KOUT::outl((int) CPU::in8(0x71));


#if TESTING_TIMER_TEST
  StdErr.print(" timer test, 3 secs...");
  for (int i = 0; i < 3; i++) {
    Timeout::sleep(Clock::now() + 1000);
    StdErr.print(' ', i+1);
  }
  StdErr.print(" done.", kendl);
#endif
#if TESTING_KEYCODE_LOOP
  Thread* t = Thread::create()->setPriority(topPriority);
  Machine::setAffinity(*t, 0);
  t->start((ptr_t)keybLoop);
#endif
  Thread::create()->start((ptr_t)UserMain);
#if TESTING_PING_LOOP
  for (;;) {
    Timeout::sleep(Clock::now() + 1000);
    KOUT::outl("...ping...");
  }
#endif
}

extern "C" void kmain(mword magic, mword addr, mword idx)         __section(".boot.text");
extern "C" void kmain(mword magic, mword addr, mword idx) {
  if (magic == 0 && addr == 0xE85250D6) {
    // low-level machine-dependent initialization on AP
    Machine::initAP(idx);
  } else {
    // low-level machine-dependent initialization on BSP -> starts kosMain
    Machine::initBSP(magic, addr, idx);
  }
}
