#include "Ham.h"
#include "WorldManager.h"
#include "MessageLib/MessageLib.h"
#include "LogManager/LogManager.h"
#include "WorldConfig.h"


//===========================================================================

Ham::Ham()
: mParent(NULL),
mBattleFatigue(0),
mBaseHitpointsUpdateCounter(9),
mNextBaseHitpointsUpdateInterval(0),
mCurrentHitpointsUpdateCounter(0),
mNextCurrentHitpointsUpdateInterval(0),
mMaxHitpointsUpdateCounter(0),
mNextMaxHitpointsUpdateInterval(0),
mWoundsUpdateCounter(9), 
mNextWoundsUpdateInterval(0),
mCurrentForce(0),
mMaxForce(0),
mRegenerating(false),
mLastRegenTick(0),
mFirstUpdateCounterChange(false),
mTaskId(0)
{ 
	HamProperty* p[] = {&mHealth,&mStrength,&mConstitution,&mAction,&mQuickness,&mStamina,&mMind,&mFocus,&mWillpower};
	mHamBars = HamBars(p,p + 9);

	int32* st[] = {&mTargetHealth,&mTargetStrength,&mTargetConstitution,&mTargetAction,&mTargetQuickness,&mTargetStamina,&mTargetMind,&mTargetFocus,&mTargetWillpower};
	mTargetStats = TargetStats(st,st + 9);
}

//===========================================================================

Ham::Ham(CreatureObject* parent,uint32 bf)
: mParent(parent),
mBattleFatigue(bf),
mBaseHitpointsUpdateCounter(0),
mNextBaseHitpointsUpdateInterval(0),
mCurrentHitpointsUpdateCounter(0),
mNextCurrentHitpointsUpdateInterval(0),
mMaxHitpointsUpdateCounter(0),
mNextMaxHitpointsUpdateInterval(0),
mWoundsUpdateCounter(0), 
mNextWoundsUpdateInterval(0),
mRegenerating(false),
mLastRegenTick(0),
mCurrentForce(0),
mMaxForce(0),
mFirstUpdateCounterChange(false)
{ 
	HamProperty* p[] = {&mHealth,&mStrength,&mConstitution,&mAction,&mQuickness,&mStamina,&mMind,&mFocus,&mWillpower};
	mHamBars = HamBars(p,p + 9);

	updateRegenRates();
}

//===========================================================================

Ham::~Ham()
{
}

//===========================================================================
//adds the interval to the updatecounter AFTER the current interval has been send
//(in case more than one hambar is updated)
//===========================================================================

uint32 Ham::getCurrentHitpointsUpdateCounter()
{
	return(mCurrentHitpointsUpdateCounter);
}

//===========================================================================
//adds the interval to the updatecounter AFTER the current interval has been send
//(in case more than one hambar is updated)
//===========================================================================

uint32 Ham::getBaseHitpointsUpdateCounter(uint32 nextInterval)
{
	mNextBaseHitpointsUpdateInterval = nextInterval;
	return mBaseHitpointsUpdateCounter;
}

//===========================================================================
//advances the updatecounter with the given interval
//===========================================================================

void Ham::advanceCurrentHitpointsUpdateCounter(uint32 amount)
{
	mCurrentHitpointsUpdateCounter += amount;  
}

//===========================================================================
//advances the updatecounter with the given interval
//===========================================================================

void Ham::advanceBaseHitpointsUpdateCounter(uint32 amount)
{
	mBaseHitpointsUpdateCounter += amount;  
	mBaseHitpointsUpdateCounter += mNextBaseHitpointsUpdateInterval; 
}

//===========================================================================

uint32 Ham::getMaxHitpointsUpdateCounter(uint32 nextInterval)
{
	mNextMaxHitpointsUpdateInterval = nextInterval;
	return mMaxHitpointsUpdateCounter;
}

//===========================================================================

void Ham::advanceMaxHitpointsUpdateCounter(uint32 amount)
{
	mMaxHitpointsUpdateCounter += amount;  
	mMaxHitpointsUpdateCounter += mNextMaxHitpointsUpdateInterval; 
}

//===========================================================================

uint32 Ham::getWoundsUpdateCounter(uint32 nextInterval)
{
	mNextWoundsUpdateInterval = nextInterval;
	return mWoundsUpdateCounter;
}

//===========================================================================

void Ham::advanceWoundsUpdateCounter(uint32 amount)
{
	mWoundsUpdateCounter += amount;  
	mWoundsUpdateCounter += mNextWoundsUpdateInterval; 
}

//===========================================================================

void Ham::updateBattleFatigue(int32 bf, bool sendUpdate)
{ 
	
	if(bf > 0 && mBattleFatigue == 1000)
	{
		return;
	}
	if(bf < 0 && mBattleFatigue == 0)
	{
		return;
	}

	mBattleFatigue += bf; 

	if(mBattleFatigue > 1000)
	{
		mBattleFatigue = 1000;
	}
	else if(mBattleFatigue < 0)
	{
		mBattleFatigue = 0;
	}

	if(sendUpdate)
		gMessageLib->sendBFUpdateCreo3(mParent);
}

//===========================================================================
// do we need this anyway ?

void Ham::setBattleFatigue(int32 bf)
{ 
	mBattleFatigue = bf; 

	if(mBattleFatigue > 1000)
	{
		mBattleFatigue = 1000;
	}
	else if(mBattleFatigue < 0)
	{
		mBattleFatigue = 0;
	}
}

//===========================================================================

void Ham::calcAllModifiedHitPoints()
{

	for(uint32 i =0;i<9;i++)
	{
		setPropertyValue(i,HamProperty_BaseHitpoints,getPropertyValue(i,HamProperty_MaxHitpoints));
	}
	
	mHealth.calcModifiedHitPoints();
	mStrength.calcModifiedHitPoints();
	mConstitution.calcModifiedHitPoints();
	mAction.calcModifiedHitPoints();
	mQuickness.calcModifiedHitPoints();
	mStamina.calcModifiedHitPoints();
	mMind.calcModifiedHitPoints();
	mFocus.calcModifiedHitPoints();
	mWillpower.calcModifiedHitPoints();
}

//===========================================================================
//
// returns the targets for a given Hambar
//

int32 Ham::getTargetStatValue(uint8 propertyIndex)
{
	return(*mTargetStats[propertyIndex]);
}

//===========================================================================

int32 Ham::getPropertyValue(uint8 propertyIndex,uint8 valueIndex)
{
	return(mHamBars[propertyIndex]->getValue(valueIndex));
}

//===========================================================================
//
// gets the totalamount of Ham points to check for cheaters while statmigrating
//

int32	Ham::getTotalHamCount()
{
	int32 amount = 0;

	for(uint32 i = 0;i<9;i++)
	{
		amount += mHamBars[i]->getValue(HamProperty_MaxHitpoints);
	}

	return amount;
}

//===========================================================================
//
// sets the target stats for a given Hambar
//

void Ham::setTargetStatValue(uint8 propertyIndex,int32 propertyValue)
{
	*mTargetStats[propertyIndex] = propertyValue;
}

//===========================================================================

void Ham::setPropertyValue(uint8 propertyIndex,uint8 valueIndex, int32 propertyValue)
{
	mHamBars[propertyIndex]->setValue(valueIndex,propertyValue);
}

//===========================================================================
//
// Updates HAM after general change of wounds, like when cloning.
// Note: The wounds may already have been updated by a DB request (directly into member vars), in this case we use a "propertyDelta" of zero.
//

void Ham::updateAllWounds(int32 propertyDelta)
{
	for (uint8 barIndex = HamBar_Health; barIndex <= HamBar_Willpower; barIndex++)
	{
		if (mHamBars[barIndex]->updateWounds(propertyDelta))
		{
			gMessageLib->sendCurrentHitpointDeltasCreo6_Single(mParent, barIndex);
		}
	}
}

//===========================================================================
//
// Updates wounds on the primary stats only, like when cloning.
//

void Ham::updatePrimaryWounds(int32 propertyDelta)
{
	for (uint8 barIndex = HamBar_Health; barIndex <= HamBar_Willpower; barIndex += 3)
	{
		if (mHamBars[barIndex]->updateWounds(propertyDelta))
		{
			gMessageLib->sendCurrentHitpointDeltasCreo6_Single(mParent, barIndex);
		}
	}
}


//===========================================================================
//
// gets called in order to modify a hambar
//

void Ham::updatePropertyValue(uint8 barIndex,uint8 valueIndex,int32 propertyDelta,bool damage,bool sendUpdate)
{
	if(propertyDelta == 0)
	{
		return;
	}

	switch(valueIndex)
	{
		case HamProperty_CurrentHitpoints:
		{
			if(sendUpdate)
			{
				switch(mHamBars[barIndex]->updateCurrentHitpoints(propertyDelta,damage))
				{
					// update went through
					case 1:
					{
						gMessageLib->sendCurrentHitpointDeltasCreo6_Single(mParent,barIndex);
					}
					break;

					// incap
					case 2:
					{
						gMessageLib->sendCurrentHitpointDeltasCreo6_Single(mParent,barIndex);

						if(mParent)
						{
							mParent->incap();
						}
					}
					break;

					default:break;
				}
			}	
		}
		break;

		case HamProperty_Wounds:
		{
			// add, need to remove hitpoints in some cases
			if(propertyDelta > 0)
			{
				// check for upper wound limit
				if(mHamBars[barIndex]->getWounds() >= mHamBars[barIndex]->getMaxHitPoints() - 1)
				{
					break;
				}

				//are the wounds properly applied? 
				//if yes modify current hitpoints accordingly
				if(mHamBars[barIndex]->updateWounds(propertyDelta) && sendUpdate)
				{
					gMessageLib->sendCurrentHitpointDeltasCreo6_Single(mParent, barIndex);
				}

				if(sendUpdate)
				{
					gMessageLib->sendWoundUpdateCreo3(mParent, barIndex);

					if(mParent->getType() == ObjType_Player)
					{
						gMessageLib->sendCombatSpam(mParent,mParent,propertyDelta,"cbt_spam","wounded");
					}
				}
			}

			//remove 
			else
			{	
				// also need to check for lower limit
				if(mHamBars[barIndex]->getWounds())
				{
					mHamBars[barIndex]->updateWounds(propertyDelta);
					if(sendUpdate)
						gMessageLib->sendWoundUpdateCreo3(mParent, barIndex);	
				}
			}
		}
		break;

		case HamProperty_BaseHitpoints:
		{
			if(mHamBars[barIndex]->updateBaseHitpoints(propertyDelta) && sendUpdate)
			{
				gMessageLib->sendCurrentHitpointDeltasCreo6_Single(mParent, barIndex);
			}

			if(sendUpdate)
			{
				gMessageLib->sendMaxHitpointDeltasCreo6_Single(mParent, barIndex);
				gMessageLib->sendBaseHitpointDeltasCreo1_Single(mParent, barIndex);
			}
		}
		break;

		case HamProperty_Modifier:
			if(mHamBars[barIndex]->updateModifiedHitpoints(propertyDelta) && sendUpdate)
			{
				gMessageLib->sendMaxHitpointDeltasCreo6_Single(mParent, barIndex);
				gMessageLib->sendCurrentHitpointDeltasCreo6_Single(mParent, barIndex);
			}
		break;

		case HamProperty_Encumbrance:
		{
			gLogger->logMsg("\ncouldnt update encumbrance\n");
			// TODO
		}
		break;

		default:{}break;
	}

	updateRegenRates();
	checkForRegen();
}

//===========================================================================
//
// gets called in order to modify the special H-bar used by vehicles and some static objects.
//

void Ham::updateSingleHam(int32 propertyDelta, bool damage)
{
	//gLogger->logMsgF("Ham::updateSingleHam with health %d", MSG_NORMAL, propertyDelta);
	if(propertyDelta == 0)
	{
		return;
	}

	switch(mHamBars[HamBar_Health]->updateCurrentHitpoints(propertyDelta,damage))
	{
		// update went through
		case 1:
		{
			// gMessageLib->sendCurrentHitpointDeltasCreo6_Single(mParent,barIndex);
			gMessageLib->sendSingleBarUpdate(mParent);
			// gLogger->logMsgF("Ham::updateSingleHam Action = 1", MSG_NORMAL);
		}
		break;

		// incap
		case 2:
		{
			// gMessageLib->sendCurrentHitpointDeltasCreo6_Single(mParent,barIndex);
			gMessageLib->sendSingleBarUpdate(mParent);
			// gLogger->logMsgF("Ham::updateSingleHam Action = 2", MSG_NORMAL);

			if(mParent)
			{
				// Npc's don't incap, they die. 
				mParent->incap();	// Handles the incap-die selection depending of object type.
				// mParent->die();
			}
		}
		break;

		default:
		{
			gLogger->logMsgF("Ham::updateSingleHam Action = Default\n", MSG_NORMAL);
		}
		break;
	}

	// Don't know if these types of objects can regenerate...?

	// Lairs generate, but we are not there yet...

	// updateRegenRates();
	// checkForRegen();
}



//===========================================================================

bool Ham::regenerate(uint64 time,void*)
{
	bool healthRegened	= true;
	bool actionRegened	= true;
	bool mindRegened	= true;
	bool forceRegened	= true;

	if(mHealth.getCurrentHitPoints() < mHealth.getModifiedHitPoints())
	{
		healthRegened = _regenHealth();
		gMessageLib->sendCurrentHitpointDeltasCreo6_Single(mParent,HamBar_Health);
	}

	if(mAction.getCurrentHitPoints() < mAction.getModifiedHitPoints())
	{
		
		//returns true if regeneration complete
		actionRegened = _regenAction();

		gMessageLib->sendCurrentHitpointDeltasCreo6_Single(mParent,HamBar_Action);
	}

	if(mMind.getCurrentHitPoints() < mMind.getModifiedHitPoints())
	{
		mindRegened = _regenMind();
		gMessageLib->sendCurrentHitpointDeltasCreo6_Single(mParent,HamBar_Mind);
	}

	if(mCurrentForce < mMaxForce)
	{
		forceRegened = _regenForce();

		if(PlayerObject* player = dynamic_cast<PlayerObject*>(mParent))
		{
			gMessageLib->sendUpdateCurrentForce(player);
		}
	}

	if(!healthRegened || !actionRegened || !mindRegened || !forceRegened)
	{
		return(true);
	}
	else
	{
		mTaskId = 0;
		return(false);
	}
}

//===========================================================================
//
// checks whether the hambar is completely regenerated
// returns true in case the regeneration is complete
//

bool Ham::_regenHealth()
{
	mHealth.updateCurrentHitpoints(mHealthRegenRate);

	if(mHealth.getCurrentHitPoints() >= mHealth.getModifiedHitPoints())
	{
		mHealth.setCurrentHitPoints(mHealth.getModifiedHitPoints());

		return(true);
	}
	return(false);
}

//===========================================================================
//
// checks whether the hambar is completely regenerated
// returns true in case the regeneration is complete
//

bool Ham::_regenAction()
{
	mAction.updateCurrentHitpoints(mActionRegenRate);

	if(mAction.getCurrentHitPoints() >= mAction.getModifiedHitPoints())
	{
		mAction.setCurrentHitPoints(mAction.getModifiedHitPoints());

		return(true);
	}
	return(false);
}

//===========================================================================
//
// checks whether the hambar is ompletely regenerated
// returns true in case the regeneration is complete
//

bool Ham::_regenMind()
{
	mMind.updateCurrentHitpoints(mMindRegenRate);

	if(mMind.getCurrentHitPoints() >= mMind.getModifiedHitPoints())
	{
		mMind.setCurrentHitPoints(mMind.getModifiedHitPoints());

		return(true);
	}
	return(false);
}

//===========================================================================
//
// regen force
//

bool Ham::_regenForce()
{
	mCurrentForce += mForceRegenRate;

	if(mCurrentForce >= mMaxForce)
	{
		mCurrentForce = mMaxForce;

		return(true);
	}

	return(false);
}

//===========================================================================
//
// FIXME
//

void Ham::updateRegenRates()
{
	float regenModifier = 1.0f;

	switch(mParent->getPosture())
	{
		case CreaturePosture_Crouched:			regenModifier = 1.25f;	break;
		case CreaturePosture_Sitting:			regenModifier = 1.75f;	break;
		case CreaturePosture_Incapacitated:	
		case CreaturePosture_Dead:				regenModifier = 0.0f;	break;
		case CreaturePosture_KnockedDown:		regenModifier = 0.75f;	break;
	}

	mHealthRegenRate	= (int32)((mConstitution.getCurrentHitPoints() / gWorldConfig->mHealthRegenDivider) * regenModifier);
	mActionRegenRate	= (int32)((mStamina.getCurrentHitPoints() / gWorldConfig->mActionRegenDivider) * regenModifier);
	mMindRegenRate		= (int32)((mWillpower.getCurrentHitPoints() / gWorldConfig->mMindRegenDivider) * regenModifier);
	mForceRegenRate		= (int32)(25.0f * regenModifier);

	// Test for creatures
	if (this->getParent())
	{
		if (this->getParent()->getType() == ObjType_Creature)
		{
			mHealthRegenRate = (mHealthRegenRate/10) + 1;
			mActionRegenRate = (mActionRegenRate/10) + 1;
			mMindRegenRate = (mMindRegenRate/10) + 1;
		}
	}
}

//===========================================================================

bool Ham::checkMainPools(int32 health,int32 action,int32 mind)
{
	if(mHealth.getCurrentHitPoints() <= health || mAction.getCurrentHitPoints() <= action || mMind.getCurrentHitPoints() <= mind)
		return(false);

	return(true);
}

//===========================================================================

void Ham::checkForRegen()
{
	if(!mTaskId
	&&(mHealth.getCurrentHitPoints()	< mHealth.getModifiedHitPoints()
	|| mAction.getCurrentHitPoints()	< mAction.getModifiedHitPoints()
	|| mMind.getCurrentHitPoints()		< mMind.getModifiedHitPoints()
	|| mCurrentForce					< mMaxForce)
	&& !mParent->isIncapacitated() && !mParent->isDead())
	{
		mTaskId = gWorldManager->addCreatureHamToProccess(this);
	}
}

//===========================================================================

void Ham::resetCounters()
{
	mBaseHitpointsUpdateCounter			= 0;
	mNextBaseHitpointsUpdateInterval	= 0;
	mCurrentHitpointsUpdateCounter		= 0;
	mNextCurrentHitpointsUpdateInterval	= 0;
	mMaxHitpointsUpdateCounter			= 0;
	mNextMaxHitpointsUpdateInterval		= 0;
	mWoundsUpdateCounter				= 0;
	mNextWoundsUpdateInterval			= 0;
	mRegenerating						= false;
	mLastRegenTick						= 0;
	mFirstUpdateCounterChange			= false;
}

//===========================================================================

bool Ham::updateCurrentForce(int32 forceDelta,bool sendUpdate)
{
	int32 newForce = mCurrentForce + forceDelta;

	if(newForce < 0)
	{
		return(false);
	}
	else if(newForce >= mMaxForce)
	{
		mCurrentForce = mMaxForce;
	}
	else
	{
		mCurrentForce = newForce;
	}

	if(sendUpdate)
	{
		if(PlayerObject* player = dynamic_cast<PlayerObject*>(mParent))
		{
			gMessageLib->sendUpdateCurrentForce(player);
		}
	}

	checkForRegen();

	return(true);
}

//===========================================================================

bool Ham::updateMaxForce(int32 forceDelta,bool sendUpdate)
{
	int32 newForce = mMaxForce + forceDelta;

	if(newForce < 1)
	{
		return(false);
	}

	if(newForce < mCurrentForce)
	{
		updateCurrentForce(newForce);
	}
	
	mMaxForce = newForce;

	if(sendUpdate)
	{
		if(PlayerObject* player = dynamic_cast<PlayerObject*>(mParent))
		{
			gMessageLib->sendUpdateMaxForce(player);
		}
	}

	return(true);
}

//===========================================================================


