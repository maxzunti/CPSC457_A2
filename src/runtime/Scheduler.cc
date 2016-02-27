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
#include "runtime/RuntimeImpl.h"
#include "runtime/Scheduler.h"
#include "runtime/Stack.h"
#include "runtime/Thread.h"
#include "kernel/Output.h"
#include "world/Access.h"
#include "machine/Machine.h"
#include "devices/Keyboard.h"	
#include "kernel/Clock.h"   
#include "runtime/globalVar.h"
#include "kernel/Kernel.h"
	   
mword Scheduler::ticksPerSecond;
mword Scheduler::defaultEpochLengthTicks;
mword Scheduler::schedMinGranularityTicks;
mword Scheduler::minvRuntime;
mword Scheduler::EpochLengthTicks;
mword Scheduler::taskTimeSlice;
/***********************************
    Used as a node in the tree to 
	reference the thread instance
	Created by: Adam Fazekas (Fall 2015)
***********************************/
class ThreadNode{
	friend class Scheduler;
	Thread *th;
	
	public:
		bool operator < (ThreadNode other) const {
			return th->priority < other.th->priority;
		}
		bool operator == (ThreadNode other) const {
			return th->priority == other.th->priority;
		}
		bool operator > (ThreadNode other) const {
			return th->priority > other.th->priority;
		}
    
	//this is how we want to do it
	ThreadNode(Thread *t){
		th = t;
	}
	static mword sumPriorities(Tree<ThreadNode> *readyTree) {
		return sumPrioritiesRecurse(readyTree->root);
	}

	static mword sumPrioritiesRecurse(Tree<ThreadNode>::node * localRoot) {
		if (!localRoot) return 0;
		return (localRoot->item.th->priority + sumPrioritiesRecurse(localRoot->r) + sumPrioritiesRecurse(localRoot->l));
	}
};	   


	   
/***********************************
			Constructor
***********************************/	   
Scheduler::Scheduler() : readyCount(0), preemption(0), resumption(0), partner(this) {
	//Initialize the idle thread
	//(It keeps the CPU awake when there are no other threads currently running)
	Thread* idleThread = Thread::create((vaddr)idleStack, minimumStack);
	idleThread->setAffinity(this)->setPriority(idlePriority);
	// use low-level routines, since runtime context might not exist
	idleThread->stackPointer = stackInit(idleThread->stackPointer, &Runtime::getDefaultMemoryContext(), (ptr_t)Runtime::idleLoop, this, nullptr, nullptr);
	
	//Initialize the tree that contains the threads waiting to be served
	readyTree = new Tree<ThreadNode>();
	
	//Add the idle thread to the tree
	readyTree->insert(*(new ThreadNode(idleThread)));
	readyCount += 1;

	Scheduler::minvRuntime = idleThread->vRuntime;
	//KOUT::outl("constructor");
	//KOUT::outl(minvRuntime);
}

/***********************************
		Static functions
***********************************/      
static inline void unlock() {}

template<typename... Args>
static inline void unlock(BasicLock &l, Args&... a) {
  l.release();
  unlock(a...);
}	   

/***********************************
    Gets called whenever a thread 
	should be added to the tree
***********************************/
void Scheduler::enqueue(Thread& t) {
  //Scheduler::minvRuntime = readyTree->readMinNode()->th->vRuntime;
  if (t.isAsleep == true) {
	t.vRuntime = t.vRuntime + Scheduler::minvRuntime;
    t.isAsleep = false;
  }  
  else {
	t.vRuntime = Scheduler::minvRuntime;  
  } 

  GENASSERT1(t.priority < maxPriority, t.priority);
  readyLock.acquire();
  readyTree->insert(*(new ThreadNode(&t)));	
  // Compute new epoch length 
 // KOUT::outl(Kernel::defaultEpochLengthTicks);
  //KOUT::outl(Scheduler::schedMinGranularityTicks);
  if (Scheduler::defaultEpochLengthTicks >= (readyCount * Scheduler::schedMinGranularityTicks)) {
	Scheduler::EpochLengthTicks = Scheduler::defaultEpochLengthTicks;
  } else {
    Scheduler::EpochLengthTicks = ((readyCount + 1) * Scheduler::schedMinGranularityTicks); 
  }
	

  bool wake = (readyCount == 0);
  readyCount += 1;						
  readyLock.release();
  Runtime::debugS("Thread ", FmtHex(&t), " queued on ", FmtHex(this));
  if (wake) Runtime::wakeUp(this);
}

/***********************************
    Gets triggered at every RTC
	interrupt (per Scheduler)
***********************************/
void Scheduler::preempt(){		// IRQs disabled, lock count inflated

	//KOUT::outl("running!");

   // KOUT::outl(Kernel::defaultEpochLengthTicks);
    //KOUT::outl(Kernel::schedMinGranularityTicks);
	//Get current running thread
	//KOUT::outl(Scheduler::schedMinGranularityTicks);
	Thread* currentThread = Runtime::getCurrThread();

	//Get its target scheduler
	Scheduler* target = currentThread->getAffinity();						
	
	//Check if the thread should move to a new scheduler
	//(based on the affinity)

	if(target != this && target){						
		//Switch the served thread on the target scheduler
		KOUT::outl("here though?");
		switchThread(target);				
	}



	//update the vRuntime //
  //  KOUT::outl("timeslice");
	//KOUT::outl(Scheduler::taskTimeSlice);
	mword updatedRuntime = currentThread->vRuntime + currentThread->timeSlice;
	currentThread->vRuntime = updatedRuntime;

	//KOUT::outl("preempt");
//	KOUT::outl(currentThread->vRuntime);
 //   KOUT::outl(Scheduler::schedMinGranularityTicks);

																						 
	/*if ((currentThread->vRuntime) >= Scheduler::schedMinGranularityTicks) { // If the currently running task already ran for minGranTicks then...
		//KOUT::outl("hi");
		if ((readyTree->readMinNode()->th->vRuntime) < (currentThread->vRuntime) && (readyTree->readMinNode()->th != currentThread)) {
			//KOUT::outl("im here!");
			//readyTree->insert(*(new ThreadNode(currentThread))); //Insert the currently running task back into the ready tree
			Scheduler::minvRuntime = readyTree->readMinNode()->th->vRuntime; // Update minvRuntime to be the leftmost nodes vRuntime
			//target = readyTree->readMinNode()->th->getAffinity();
			//KOUT::outl("Before switch:");
			//KOUT::outl(currentThread->getAffinity());
			//KOUT::outl(target);
			KOUT::outl("Switching to ", this, "    Target = ", target );
				switchThread(this); //Switch the served thread on the target scheduler
		}	
		else {
			// Continue running with the currently running task
		}
	}	
	else {
		// Continue running with the currently running task
	}
*/
	
	
	//Check if it is time to switch the thread on the current scheduler
	if(switchTest(currentThread)){
		//Switch the served thread on the current scheduler
		switchThread(this);	
	}

}

/***********************************
    Checks if it is time to stop
	serving the current thread
	and start serving the next
	one
***********************************/
bool Scheduler::switchTest(Thread* t){ //TODO: Here we must compare the vRuntime of the leftmost node to the current threads runtime 
	/*t->vRuntime++;
	if (t->vRuntime % 10 == 0)
		return true;
	return false;		*/													//Otherwise return that the thread should not be switched

	if ((t->vRuntime) >= Scheduler::schedMinGranularityTicks) { // If the currently running task already ran for minGranTicks then...
		KOUT::outl("sMGT = ", Scheduler::schedMinGranularityTicks, "     t->vRuntime = ", t->vRuntime);
		if ((readyTree->readMinNode()->th->vRuntime) < (t->vRuntime)) {
			Scheduler::minvRuntime = readyTree->readMinNode()->th->vRuntime; // Update minvRuntime to be the leftmost nodes vRuntime
			return true;
		}	
	}	
}

/***********************************
    Switches the current running
	thread with the next thread
	waiting in the tree
***********************************/
template<typename... Args>
inline void Scheduler::switchThread(Scheduler* target, Args&... a) {
  preemption += 1;
  CHECK_LOCK_MIN(sizeof...(Args));
  Thread* nextThread;
  readyLock.acquire();


	
  if(!readyTree->empty()){
	  //KOUT::("hi");
	  nextThread = readyTree->popMinNode()->th;	
      readyCount -= 1;
 	  goto threadFound;
	}

  readyLock.release();
  GENASSERT0(target);
  GENASSERT0(!sizeof...(Args));
  return;                                         // return to current thread

threadFound:
  readyLock.release();
  resumption += 1;
  Thread* currThread = Runtime::getCurrThread();

 // KOUT::outl("we here?");
  //KOUT::outl(&(readyTree->readMinNode()->th));
 // KOUT::outl(&(nextThread));
 // KOUT::outl(&(currThread));
 // KOUT::outl(&(currThread));
  GENASSERTN(currThread && nextThread && nextThread != currThread, currThread, ' ', nextThread);


  //KOUT::outl("???");
  if (target) currThread->nextScheduler = target; // yield/preempt to given processor
  else currThread->nextScheduler = this;          // suspend/resume to same processor
	  currThread->isAsleep = true; // Added ~~~~~
      currThread->vRuntime = (currThread->vRuntime) - (Scheduler::minvRuntime); // Added ~~~	  

  unlock(a...);                                   // ...thus can unlock now
  CHECK_LOCK_COUNT(1);
  Runtime::debugS("Thread switch <", (target ? 'Y' : 'S'), ">: ", FmtHex(currThread), '(', FmtHex(currThread->stackPointer), ") to ", FmtHex(nextThread), '(', FmtHex(nextThread->stackPointer), ')');

  Runtime::MemoryContext& ctx = Runtime::getMemoryContext();
  Runtime::setCurrThread(nextThread);

  // Calculate the tasks time slice //

  // Insert code here to add up all the priorities in readyTree
  if(!readyTree->empty()){
    //KOUT::outl("if1");
  	//Scheduler::taskTimeSlice = (Scheduler::EpochLengthTicks * (nextThread->priority + 1)) / ThreadNode::sumPriorities(readyTree);
	nextThread->timeSlice = (Scheduler::EpochLengthTicks * (nextThread->priority + 1)) / ThreadNode::sumPriorities(readyTree);
  }
  else { 
  //  KOUT::outl("b4");
	//KOUT::outl(Scheduler::EpochLengthTicks);
	//Scheduler::taskTimeSlice = (Scheduler::EpochLengthTicks * (nextThread->priority + 1));
	nextThread->timeSlice = (Scheduler::EpochLengthTicks * (nextThread->priority + 1));
   //KOUT::outl("after:");
	//KOUT::outl(Scheduler::taskTimeSlice);
  }
  KOUT::outl("lol");
  //KOUT::outl(currThread);
  //KOUT::outl(nextThread);
  Thread* prevThread = stackSwitch(currThread, target, &currThread->stackPointer, nextThread->stackPointer);


  KOUT::outl("2");
  // REMEMBER: Thread might have migrated from other processor, so 'this'
  //           might not be currThread's Scheduler object anymore.
  //           However, 'this' points to prevThread's Scheduler object.
  Runtime::postResume(false, *prevThread, ctx);
  //KOUT::outl("3");

  if (currThread->state == Thread::Cancelled) {
    currThread->state = Thread::Finishing;
    switchThread(nullptr);
    unreachable();
  }
  KOUT::outl("here?");

}

/***********************************
    Gets triggered when a thread is 
	suspended
***********************************/
void Scheduler::suspend(BasicLock& lk) {
  Runtime::FakeLock fl;
  switchThread(nullptr, lk);
}
void Scheduler::suspend(BasicLock& lk1, BasicLock& lk2) {
  Runtime::FakeLock fl;
  switchThread(nullptr, lk1, lk2);
}

/***********************************
    Gets triggered when a thread is 
	awake after suspension
***********************************/
void Scheduler::resume(Thread& t) {
  GENASSERT1(&t != Runtime::getCurrThread(), Runtime::getCurrThread());
  if (t.nextScheduler) t.nextScheduler->enqueue(t);
  else Runtime::getScheduler()->enqueue(t);
}

/***********************************
    Gets triggered when a thread is 
	done but not destroyed yet
***********************************/
void Scheduler::terminate() {
  Runtime::RealLock rl;
  Thread* thr = Runtime::getCurrThread();
  GENASSERT1(thr->state != Thread::Blocked, thr->state);
  thr->state = Thread::Finishing;
  switchThread(nullptr);
  unreachable();
}

/***********************************
		Other functions
***********************************/      
extern "C" Thread* postSwitch(Thread* prevThread, Scheduler* target) {
  CHECK_LOCK_COUNT(1);
  if fastpath(target) Scheduler::resume(*prevThread);
  return prevThread;
}

extern "C" void invokeThread(Thread* prevThread, Runtime::MemoryContext* ctx, funcvoid3_t func, ptr_t arg1, ptr_t arg2, ptr_t arg3) {
  Runtime::postResume(true, *prevThread, *ctx);
  func(arg1, arg2, arg3);
  Runtime::getScheduler()->terminate();
}





