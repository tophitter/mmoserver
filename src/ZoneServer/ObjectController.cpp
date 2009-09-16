/*
---------------------------------------------------------------------------------------
This source file is part of swgANH (Star Wars Galaxies - A New Hope - Server Emulator)
For more information, see http://www.swganh.org


Copyright (c) 2006 - 2008 The swgANH Team

---------------------------------------------------------------------------------------
*/

#include "ObjectController.h"
#include "ObjectControllerOpcodes.h"
#include "ObjectControllerCommandMap.h"
#include "WorldManager.h"
#include "CombatManager.h"
#include "DatabaseManager/Database.h"
#include "DatabaseManager/DataBinding.h"
#include "DatabaseManager/DatabaseResult.h"
#include "Common/MessageFactory.h"
#include "Common/Message.h"
#include "MessageLib/MessageLib.h"
#include "LogManager/LogManager.h"
#include "WorldConfig.h"
#include "EVQueueSize.h"
#include "EVCmdProperty.h"
#include "EVAbility.h"
#include "EVState.h"
#include "EVSurveySample.h"
#include "EVWeapon.h"
#include "EVPosture.h"
#include "PVHam.h"
#include "PVState.h"
#include "PVPosture.h"


//=============================================================================
//
// Constructor
//

ObjectController::ObjectController() :
mObject(NULL),
mTaskId(0),
mCommandQueueProcessTimeLimit(2),
mEventQueueProcessTimeLimit(2),
mNextCommandExecution(0),
mUnderrunTime(0),
mDatabase(gWorldManager->getDatabase()),
mCmdMsgPool(sizeof(ObjControllerCommandMessage)),
mEventPool(sizeof(ObjControllerEvent)),
mDBAsyncContainerPool(sizeof(ObjControllerAsyncContainer)),
mUpdatingObjects(false),
mDestroyOutOfRangeObjects(false),
mMovementInactivityTrigger(5),
mFullUpdateTrigger(0)
{
	mSI		= gWorldManager->getSI();
	// We do have a global clock object, don't use seperate clock and times for every process.
	// mClock	= new Anh_Utils::Clock();
}

//=============================================================================
//
// Constructor
//

ObjectController::ObjectController(Object* object) :
mObject(object),
mTaskId(0),
mCommandQueueProcessTimeLimit(2),
mEventQueueProcessTimeLimit(2),
mNextCommandExecution(0),
mUnderrunTime(0),
mDatabase(gWorldManager->getDatabase()),
mCmdMsgPool(sizeof(ObjControllerCommandMessage)),
mEventPool(sizeof(ObjControllerEvent)),
mDBAsyncContainerPool(sizeof(ObjControllerAsyncContainer)),
mUpdatingObjects(false),
mDestroyOutOfRangeObjects(false),
mMovementInactivityTrigger(5),
mFullUpdateTrigger(0)
{
	mSI		= gWorldManager->getSI();
}

//=============================================================================
//
// Deconstructor
//

ObjectController::~ObjectController()
{
	clearQueues();

	EnqueueValidators::iterator it = mEnqueueValidators.begin();

	while(it != mEnqueueValidators.end())
	{
		delete(*it);
		it = mEnqueueValidators.erase(it);
	}

	ProcessValidators::iterator pIt = mProcessValidators.begin();

	while(pIt != mProcessValidators.end())
	{
		delete(*pIt);
		pIt = mProcessValidators.erase(pIt);
	}
}

//=============================================================================
//
// clear queues
//

void ObjectController::clearQueues()
{
	// command queue

	CommandQueue::iterator cmdIt = mCommandQueue.begin();

	while(cmdIt != mCommandQueue.end())
	{
		ObjControllerCommandMessage* cmdMsg = (*cmdIt);

		cmdMsg->~ObjControllerCommandMessage();
		mCmdMsgPool.free(cmdMsg);

		cmdIt = mCommandQueue.erase(cmdIt);
	}

	// mCommandQueue.clear(); // Will not free the boost shit used (mCmdMsgPool).

	// event queue
	EventQueue::iterator eventIt = mEventQueue.begin();

	while(eventIt != mEventQueue.end())
	{
		ObjControllerEvent* event = (*eventIt);

		event->~ObjControllerEvent();
		mEventPool.free(event);

		eventIt = mEventQueue.erase(eventIt);
	}
}

//=============================================================================
//
// process
//

bool ObjectController::process(uint64 callTime,void*)
{
	if(!_processCommandQueue() && !_processEventQueue())
	{
		mTaskId = 0;
		return(false);
	}

	return(true);
}

//=============================================================================
//
// remove message from queue
//

void ObjectController::removeMsgFromCommandQueue(uint32 opcode)
{
	CommandQueue::iterator cmdIt = mCommandQueue.begin();

	while (cmdIt != mCommandQueue.end())
	{
		ObjControllerCommandMessage* cmdMsg = (*cmdIt);

		if(cmdMsg->getOpcode() == opcode)
		{
			cmdMsg->~ObjControllerCommandMessage();
			mCmdMsgPool.free(cmdMsg);
			cmdIt = mCommandQueue.erase(cmdIt);
		}
		else
			++cmdIt;
	}
}

//=============================================================================
//
// process command queue
//
bool ObjectController::_processCommandQueue()
{
	mHandlerCompleted = false;
	// init timers
	uint64	startTime		= Anh_Utils::Clock::getSingleton()->getLocalTime();
	uint64	currentTime		= startTime;
	uint64	processTime		= 0;
	
	PlayerObject* player  = dynamic_cast<PlayerObject*>(mObject);
	if (!player)
	{
		gLogger->logMsgF("ObjectController::_processCommandQueue() Invalid object", MSG_NORMAL);
		assert(false);
		return false;
	}

	// gLogger->logMsgF("ObjectController::_processCommandQueue() Entering at  = %llu", MSG_NORMAL, currentTime);

	// If queue empty and we are in combat, insert autoattack.
	if (mCommandQueue.empty() &&  player->autoAttackEnabled())
	{
		// Auto attack current target.
		uint64 autoTargetId = player->getCombatTargetId();
		if (autoTargetId == 0)
		{
			// We lost current target.
			// gLogger->logMsgF("We lost current target.", MSG_NORMAL);
			autoTargetId = player->getNearestDefender();
			/*
			if (autoTargetId != 0)
			{
				// We got us a new target.
				gLogger->logMsgF("We got us a new target, %llu", MSG_NORMAL, autoTargetId);
				// player->setTarget(NULL);
				player->setTarget(autoTargetId);
				gMessageLib->sendTargetUpdateDeltasCreo6(player);
			}
			*/
		}
		if (autoTargetId != 0)
		{
			this->enqueueAutoAttack(autoTargetId);
		}
	}

	// loop until empty or our time is up
	while (mCommandQueue.size() && processTime < mCommandQueueProcessTimeLimit)
	{
		// see if we got something to execute yet
		ObjControllerCommandMessage* cmdMsg = mCommandQueue.front();

		if (cmdMsg && mNextCommandExecution <= currentTime)
		{
			// gLogger->logMsgF("Executing command at = %llu", MSG_NORMAL, currentTime);

			// gLogger->logMsgF("Execution time = %llu", MSG_NORMAL, mNextCommandExecution);
			// gLogger->logMsgF("Current time   = %llu", MSG_NORMAL, currentTime);

			// Compensate for any lag, i.e. command arriving late.
			mUnderrunTime += (currentTime - mNextCommandExecution);

			//gLogger->logMsgF("Current Time %lld ExecTime %lld",MSG_LOW,currentTime,cmdMsg->getExecutionTime());
			// gLogger->logMsgF("Lag compensation will be = %llu", MSG_NORMAL, mUnderrunTime);

			// get the commands data
			Message*	message		= cmdMsg->getData();	// Be aware, internally created messages are NULL.
			uint32		command		= cmdMsg->getOpcode();
			uint64		targetId	= cmdMsg->getTargetId();
			uint32		reply1		= 0;
			uint32		reply2		= 0;
			bool		consumeHam	= true;

			ObjectControllerCmdProperties*	cmdProperties = cmdMsg->getCmdProperties();

			// validate if we are still able to execute
			if (cmdProperties && _validateProcessCommand(reply1,reply2,targetId,command,cmdProperties))
			{
				// gLogger->logMsgF("Executing command at = %llu", MSG_NORMAL, currentTime);

				uint64 timeToNextCommand = 0;

				// Set up the cooldown time.
				if (mUnderrunTime < cmdProperties->mDefaultTime)
				{
					timeToNextCommand = (cmdProperties->mDefaultTime - mUnderrunTime);
					mUnderrunTime = 0;
				}
				else
				{
					// Compensate as much as we can.
					timeToNextCommand = cmdProperties->mDefaultTime / 2;
					mUnderrunTime -= timeToNextCommand;
				}

				bool internalCommand = false;
				if (!message)
				{
					internalCommand = true;
				}

				// keep a pointer to the start
				uint16	paramsIndex = 0;
				if (message)
				{
					paramsIndex = message->getIndex();
				}

				bool cmdExecutedOk = true;

				// call the proper handler
				switch(cmdProperties->mCmdGroup)
				{
					case ObjControllerCmdGroup_Common:
					{
						// get the command
						CommandMap::iterator it = gObjControllerCmdMap.find(command);

						if (message && it != gObjControllerCmdMap.end())
						{
							(this->*((*it).second))(targetId,message,cmdProperties);
							consumeHam = mHandlerCompleted;
						}
						else
						{
							gLogger->logMsgF("ObjectController::processCommandQueue: ObjControllerCmdGroup_Common Unhandled Cmd 0x%x for %lld",MSG_NORMAL,command,mObject->getId());
							//gLogger->hexDump(message->getData(),message->getSize());

							consumeHam = false;
						}
					}
					break;

					case ObjControllerCmdGroup_Attack:
					{
						// gLogger->logMsgF("ObjectController::processCommandQueue: ObjControllerCmdGroup_Attack Handled Cmd 0x%x for %lld",MSG_NORMAL,command,mObject->getId());
						// If player activated combat or me returning fire, the peace is ended, and auto-attack allowed.
						player->enableAutoAttack();

						cmdExecutedOk = gCombatManager->handleAttack(player, targetId, cmdProperties);
						if (!cmdExecutedOk)
						{
							// gLogger->logMsg("ObjectController::processCommandQueue: handleAttack error");
							// reset current combat target.
							player->setCombatTargetId(0);

							consumeHam = mHandlerCompleted;
						}
						else
						{
							// Keep track of the target we are attacking, it's not always your "look-at" target.
							player->setCombatTargetId(targetId);
						}
					}
					break;

					default:
					{
						gLogger->logMsgF("ObjectController::processCommandQueue: Default Unhandled CmdGroup %u for %lld",MSG_NORMAL,cmdProperties->mCmdGroup,mObject->getId());

						consumeHam = false;
					}
					break;
				}
			
				if (cmdExecutedOk)
				{
					mNextCommandExecution = currentTime + timeToNextCommand;
					// gLogger->logMsgF("Setting up next command in %llu, at %llu", MSG_NORMAL, timeToNextCommand, mNextCommandExecution);
				}
				else if (internalCommand)
				{
					// we will not spam the command queue if auto-attack is set to an invalid target.
					mNextCommandExecution = currentTime + timeToNextCommand;
					// gLogger->logMsgF("Skipped internal command, setting up next command in %llu, at %llu", MSG_NORMAL, (uint64)0, mNextCommandExecution);
				}
				else
				{
					// gLogger->logMsgF("Skipped current command, setting up next command in %llu, at %llu", MSG_NORMAL, (uint64)0, mNextCommandExecution);
				}

				if(consumeHam && !_consumeHam(cmdProperties))
				{
					//gLogger->logMsgF("ObjectController::processCommandQueue: consume ham fail");
				}

				// execute any attached scripts
				if (message)	// Internally generated commands have no message body
				{
					string params;
					message->setIndex(paramsIndex);
					message->getStringUnicode16(params);
					params.convert(BSTRType_ANSI);

					gObjectControllerCommands->mCmdScriptListener.handleScriptEvent(cmdProperties->mCommandStr.getAnsi(),params);
				}
			}
			else
			{
				// Command not allowed.
				gLogger->logMsgF("Dumping command at = %llu", MSG_NORMAL, currentTime);
			}
		
			//its processed, so ack and delete it
			if (message)
			{
				// if (PlayerObject* player = dynamic_cast<PlayerObject*>(mObject))
				gMessageLib->sendCommandQueueRemove(cmdMsg->getSequence(),0.0f,reply1,reply2,player);
			}

			// Remove the command from queue. Note: pop() invokes object destructor.
			mCommandQueue.pop_front();

			// cmdMsg->~ObjControllerCommandMessage();
			mCmdMsgPool.free(cmdMsg);
		}
		// break out, if we dont get to execute something this frame
		else if((cmdMsg->getExecutionTime() - currentTime) > (mCommandQueueProcessTimeLimit - processTime))
		{
			// gLogger->logMsgF("Re-schedule Task", MSG_NORMAL);
			break;
		}

		// update timers
		currentTime = Anh_Utils::Clock::getSingleton()->getLocalTime();
		processTime = currentTime - startTime;
	}

	// if we didn't manage to process all or theres an event still waiting, don't remove us from the scheduler
	// We need to keep the queue as long as we are in combat.
	return ((mCommandQueue.size() ||  player->autoAttackEnabled())? true : false);
}

//=============================================================================
//
// process event queue
//

bool ObjectController::_processEventQueue()
{
	// init timers
	uint64	startTime		= Anh_Utils::Clock::getSingleton()->getLocalTime();
	uint64	currentTime		= startTime;
	uint64	processTime		= 0;

	// loop until empty or our time is up
	while(mEventQueue.size() && processTime < mEventQueueProcessTimeLimit)
	{
		// see if we got something to execute yet
		ObjControllerEvent* event = mEventQueue.top();

		if(event && event->getExecutionTime() <= currentTime)
		{
			mEventQueue.pop();

			mObject->handleEvent(event->getEvent());

			//its processed, delete it
			event->~ObjControllerEvent();
			mEventPool.free(event);
		}
		// break out, if we dont get to execute something this frame
		else if((event->getExecutionTime() - currentTime) > (mEventQueueProcessTimeLimit - processTime))
			break;

		// update timers
		currentTime = Anh_Utils::Clock::getSingleton()->getLocalTime();
		processTime = currentTime - startTime;
	}

	// if we didn't manage to process all, don't remove us from the scheduler
	return(mEventQueue.size() ? true : false);
}

//=============================================================================
//
// enqueue message
//
// Add the command in the queue. No fancy stuff like predicting execution time etc... just add the fracking command.
//
void ObjectController::enqueueCommandMessage(Message* message)
{
	uint32	clientTicks		= message->getUint32();
	uint32	sequence		= message->getUint32();
	uint32	opcode			= message->getUint32();
	uint64	targetId		= message->getUint64();
	uint32	reply1			= 0;
	uint32	reply2			= 0;
	
	ObjectControllerCmdProperties* cmdProperties = NULL;

	// gLogger->logMsgF("ObjController enqueue tick: %u counter: 0x%4x",MSG_NORMAL,clientTicks,sequence);
	
	if (_validateEnqueueCommand(reply1,reply2,targetId,opcode,cmdProperties))
	{
		// schedule it for immidiate execution initially
		// uint64 execTime	= Anh_Utils::Clock::getSingleton()->getLocalTime();

		// make a copy of the message, since we may need to keep it for a while
		gMessageFactory->StartMessage();
		gMessageFactory->addData(message->getData(),message->getSize());

		Message* newMessage = gMessageFactory->EndMessage();
		newMessage->setIndex(message->getIndex());	

		// create the queued message, need setters since boost pool constructor templates take 3 params max

		// The cmdProperties->mDefaultTime is NOT a delay for NEXT command, it's the cooldown for THIS command.
		ObjControllerCommandMessage* cmdMsg = new(mCmdMsgPool.malloc()) ObjControllerCommandMessage(opcode,cmdProperties->mDefaultTime,targetId);
		cmdMsg->setSequence(sequence);
		cmdMsg->setData(newMessage);
		cmdMsg->setCmdProperties(cmdProperties);

		// Do we have any commands in the queue?
		if (mCommandQueue.empty())
		{
			// No, this will now become the very top command in the queue.

			// Are there any cooldown left from previous command?
			uint64 now = Anh_Utils::Clock::getSingleton()->getLocalTime();
			if (mNextCommandExecution <= now)
			{
				uint64 underrun = now - mNextCommandExecution;

				if (underrun <= mUnderrunTime)
				{
					mUnderrunTime -= underrun;
				}
				else
				{
					mUnderrunTime = 0;
				}

				// This command are to be handled as fast as possible.
				mNextCommandExecution = now;
			}
		}
		// add it
		if (sequence && cmdProperties->mAddToCombatQueue)
		{
			// gLogger->logMsgF("Command with cooldown = %llu", MSG_NORMAL, cmdProperties->mDefaultTime);
			mCommandQueue.push_back(cmdMsg);
		}
		else
		{
			// gLogger->logMsgF("Command with priority, cooldown = %llu", MSG_NORMAL, cmdProperties->mDefaultTime);
			mCommandQueue.push_front(cmdMsg);
		}

		// add us to the scheduler, if we aren't queued already
		if(!mTaskId)
		{
			mTaskId = gWorldManager->addObjControllerToProcess(this);
		}
	}
	// not qualified for this command, so remove it
	else if(PlayerObject* player = dynamic_cast<PlayerObject*>(mObject))
	{
		gMessageLib->sendCommandQueueRemove(sequence,0.0f,reply1,reply2,player);
	}
}

//=============================================================================
//
// enqueue an internal created message (autoattack)
//
//

void ObjectController::enqueueAutoAttack(uint64 targetId)
{
	// gLogger->logMsgF("ObjectController::enqueueAutoAttack() Entering...", MSG_NORMAL);
	if (mCommandQueue.empty())
	{
		uint32 sequence = 0;
		uint32 opcode = opOCattack;

		CreatureObject* creature = dynamic_cast<CreatureObject*>(mObject);
		if (!creature)
		{
			gLogger->logMsgF("ObjectController::enqueueAutoAttack() Invalid object", MSG_NORMAL);
			assert(false);
		}

		uint32	reply1 = 0;
		uint32	reply2 = 0;
		
		ObjectControllerCmdProperties* cmdProperties = NULL;


		if (_validateEnqueueCommand(reply1,reply2,targetId,opcode,cmdProperties))
		{
			ObjControllerCommandMessage* cmdMsg = new(mCmdMsgPool.malloc()) ObjControllerCommandMessage(opcode, cmdProperties->mDefaultTime, targetId);
			cmdMsg->setSequence(sequence);
			cmdMsg->setData(NULL);
			cmdMsg->setCmdProperties(cmdProperties);

			// Are there any cooldown left from previous command?
			uint64 now = Anh_Utils::Clock::getSingleton()->getLocalTime();
			if (mNextCommandExecution <= now)
			{
				uint64 underrun = now - mNextCommandExecution;

				if (underrun <= mUnderrunTime)
				{
					mUnderrunTime -= underrun;
				}
				else
				{
					mUnderrunTime = 0;
				}

				// This command are to be handled as fast as possible.
				mNextCommandExecution = now;
			}

			// add it
			mCommandQueue.push_front(cmdMsg);

			// add us to the scheduler, if we aren't queued already
			if(!mTaskId)
			{
				mTaskId = gWorldManager->addObjControllerToProcess(this);
			}
		}
		// not qualified for this command
		else
		{
			PlayerObject* player = dynamic_cast<PlayerObject*>(mObject);
			if (player)
			{
				player->disableAutoAttack();
				gLogger->logMsgF("ObjectController::enqueueAutoAttack() Error adding command.", MSG_NORMAL);
			}
		}
	}
}


//=============================================================================
//
// add an event
//

void ObjectController::addEvent(Anh_Utils::Event* event,uint64 timeDelta)
{
	ObjControllerEvent* ocEvent = new(mEventPool.malloc()) ObjControllerEvent(Anh_Utils::Clock::getSingleton()->getLocalTime() + timeDelta,event);

	// add it
	mEventQueue.push(ocEvent);

	// add us to the scheduler, if we aren't queued already
	if(!mTaskId)
	{
		mTaskId = gWorldManager->addObjControllerToProcess(this);
	}
}

//=============================================================================
//
// remove command message
//

void ObjectController::removeCommandMessage(Message* message)
{
	// skip tickcount
	message->getUint32();

	// pass sequence
	removeMsgFromCommandQueueBySequence(message->getUint32());
}

//=============================================================================
//
// remove a cmd queue message by its sequence given
//
// Note by Eru:
// if a command already have been executed, you can not remove the cooldown for that command. And you should not expect to find the already executed command still in the queue.
// if a command has not ben exected yet (waiting in queue), there is no delay set for THAT command, thus no cooldown time to remove.
// Just remove the command!


void ObjectController::removeMsgFromCommandQueueBySequence(uint32 sequence)
{
	// sanity check
	if (!sequence)
	{
		return;
	}

	CommandQueue::iterator cmdIt = mCommandQueue.begin();

	while(cmdIt != mCommandQueue.end())
	{
		if((*cmdIt)->getSequence() == sequence)
		{
			ObjControllerCommandMessage* msg = (*cmdIt);

			// delete it
			msg->~ObjControllerCommandMessage();
			mCmdMsgPool.free(msg);

			mCommandQueue.erase(cmdIt);
			break;
		}
		++cmdIt;
	}
}

//=============================================================================
//
// checks if a command is allowed to be executed
// sets the according error replies
//

bool ObjectController::_validateEnqueueCommand(uint32 &reply1,uint32 &reply2,uint64 targetId,uint32 opcode,ObjectControllerCmdProperties*& cmdProperties)
{
	EnqueueValidators::iterator it = mEnqueueValidators.begin();

	while(it != mEnqueueValidators.end())
	{
		if(!((*it)->validate(reply1,reply2,targetId,opcode,cmdProperties)))
		{
			return(false);
		}

		++it;
	}

	return(true);
}

//=============================================================================
//
// checks if a command is allowed to be executed
// sets the according error replies
//

bool ObjectController::_validateProcessCommand(uint32 &reply1,uint32 &reply2,uint64 targetId,uint32 opcode,ObjectControllerCmdProperties*& cmdProperties)
{
	ProcessValidators::iterator it = mProcessValidators.begin();

	while(it != mProcessValidators.end())
	{
		if(!((*it)->validate(reply1,reply2,targetId,opcode,cmdProperties)))
		{
			return(false);
		}

		++it;
	}

	return(true);
}

//=============================================================================
//
// setup enqueue cmd validators
// make sure to keep the order sane
//

void ObjectController::initEnqueueValidators()
{
	mEnqueueValidators.push_back(new EVQueueSize(this));
	mEnqueueValidators.push_back(new EVCmdProperty(this));

	switch(mObject->getType())
	{
		case ObjType_Player:
		{
			mEnqueueValidators.push_back(new EVSurveySample(this));
		}
		case ObjType_NPC:
		case ObjType_Creature:
		{
			mEnqueueValidators.push_back(new EVPosture(this));
			mEnqueueValidators.push_back(new EVAbility(this));
			//mEnqueueValidators.push_back(new EVState(this));
			mEnqueueValidators.push_back(new EVWeapon(this));
		}
		break;

		default: break;
	}
}

//=============================================================================
//
// setup process cmd validators
// make sure to keep the order sane
//

void ObjectController::initProcessValidators()
{
	switch(mObject->getType())
	{
		case ObjType_Player:
		case ObjType_NPC:
		case ObjType_Creature:
		{
			mProcessValidators.push_back(new PVPosture(this));
			mProcessValidators.push_back(new PVHam(this));
			mProcessValidators.push_back(new PVState(this));
		}
		break;

		default: break;
	}
}

//=============================================================================

bool ObjectController::_consumeHam(ObjectControllerCmdProperties* cmdProperties)
{
	if(CreatureObject* creature	= dynamic_cast<CreatureObject*>(mObject))
	{
		if(Ham* ham = creature->getHam())
		{
			if(cmdProperties->mHealthCost)
			{
				ham->updatePropertyValue(HamBar_Health,HamProperty_CurrentHitpoints,-cmdProperties->mHealthCost);
			}

			if(cmdProperties->mActionCost)
			{
				ham->updatePropertyValue(HamBar_Action,HamProperty_CurrentHitpoints,-cmdProperties->mActionCost);
			}

			if(cmdProperties->mMindCost)
			{
				ham->updatePropertyValue(HamBar_Mind,HamProperty_CurrentHitpoints,-cmdProperties->mMindCost);
			}
		}
		else
		{
			return(false);
		}

		return(true);
	}

	return(false);
}