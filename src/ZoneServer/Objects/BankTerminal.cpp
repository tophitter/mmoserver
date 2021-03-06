/*
---------------------------------------------------------------------------------------
This source file is part of SWG:ANH (Star Wars Galaxies - A New Hope - Server Emulator)

For more information, visit http://www.swganh.com

Copyright (c) 2006 - 2014 The SWG:ANH Team
---------------------------------------------------------------------------------------
Use of this source code is governed by the GPL v3 license that can be found
in the COPYING file or at http://www.gnu.org/licenses/gpl-3.0.html

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
---------------------------------------------------------------------------------------
*/
#include "ZoneServer/Objects/BankTerminal.h"
#include "Zoneserver/Objects/Bank.h"
#include "Zoneserver/Objects/Inventory.h"
#include "ZoneServer/Objects/Player Object/PlayerObject.h"
#include "ZoneServer/GameSystemManagers/Treasury Manager/TreasuryManager.h"
#include "ZoneServer/GameSystemManagers/UI Manager/UIManager.h"
#include "MessageLib/MessageLib.h"


#include "ZoneServer\WorldManager.h"
#include "ZoneServer\Services\equipment\equipment_service.h"

//=============================================================================

BankTerminal::BankTerminal() : Terminal()
{
}

//=============================================================================

BankTerminal::~BankTerminal()
{
}

//=============================================================================
void BankTerminal::handleObjectMenuSelect(uint8 messageType, Object* srcObject)
{
    PlayerObject* playerObject = (PlayerObject*)srcObject;

    switch(messageType)
    {

    case radId_bankDepositAll: // deposit all

        gTreasuryManager->bankDepositAll(playerObject);

        break;


    case radId_bankWithdrawAll: // withdraw all

        gTreasuryManager->bankWithdrawAll(playerObject);

        break;


    case radId_itemUse:
    case radId_bankTransfer:	{ // deposit - withdraw
	
		auto equip_service = gWorldManager->getKernel()->GetServiceManager()->GetService<swganh::equipment::EquipmentService>("EquipmentService");
		auto inventory	= dynamic_cast<Inventory*>(equip_service->GetEquippedObject(playerObject->GetCreature(), "inventory"));
		auto bank		= dynamic_cast<Bank*>(equip_service->GetEquippedObject(playerObject->GetCreature(), "bank"));

        gUIManager->createNewTransferBox(this,"handleDepositWithdraw", "@base_player:bank_title"
                                         ,"@base_player:bank_prompt", "Cash", "Bank"
										 ,inventory->getCredits()
                                         ,bank->getCredits()
                                         ,playerObject);
	}

    break;


    case radId_bankItems:

        gTreasuryManager->bankOpenSafetyDepositContainer(playerObject);

        break;


    case radId_bankJoin: // join

        gTreasuryManager->bankJoin(playerObject);

        break;


    case radId_bankQuit: // quit

        gTreasuryManager->bankQuit(playerObject);

        break;


    default:

        break;
    }

}


//=============================================================================

void BankTerminal::handleUIEvent(BString strInventoryCash, BString strBankCash, UIWindow* window)
{
	// if Window is equal to NULL -> Can not open window
    if(window == NULL)
    {
        return;
    }

	PlayerObject* playerObject = window->getOwner(); // window owner

	// Check if you can use the bank
    if(playerObject == NULL || !playerObject->isConnected() || playerObject->getSamplingState() || playerObject->GetCreature()->isIncapacitated() || playerObject->GetCreature()->isDead() ||		playerObject->GetCreature()->states.checkState(CreatureState_Combat))    {
		gMessageLib->SendSystemMessage(L"You can not use the bank at your current state", playerObject); // Temp Message
        return;
    }

	auto equip_service = gWorldManager->getKernel()->GetServiceManager()->GetService<swganh::equipment::EquipmentService>("EquipmentService");
	auto inventory	= dynamic_cast<Inventory*>(equip_service->GetEquippedObject(playerObject->GetCreature(), "inventory"));
	auto bank		= dynamic_cast<Bank*>(equip_service->GetEquippedObject(playerObject->GetCreature(), "bank"));

    // two money movement deltas stands for credits
    // variations into bank & inventory.
    // casting as signed cause one will be negative.
    // when inventoryDelta + bankDelta is not equal to zero,
    // that means player treasury has changed since
    // the transfer window opened.

    // we get the money deltas by parsing the string returned
    // by the SUI window

    strInventoryCash.convert(BSTRType_ANSI);
    strBankCash.convert(BSTRType_ANSI);

    int32 inventoryMoneyDelta = atoi(strInventoryCash.getAnsi()) - inventory->getCredits();
    int32 bankMoneyDelta = atoi(strBankCash.getAnsi()) - bank->getCredits();

    // the amount transfered must be greater than zero
    if(bankMoneyDelta == 0 || inventoryMoneyDelta == 0)
    {
        return;
    }

    gTreasuryManager->bankTransfer(inventoryMoneyDelta, bankMoneyDelta, playerObject);
}

//=============================================================================

void BankTerminal::prepareRadialMenu()
{
}

//=============================================================================

void BankTerminal::prepareCustomRadialMenu(CreatureObject* creatureObject, uint8 itemCount)
{
    mRadialMenu = gTreasuryManager->bankBuildTerminalRadialMenu(creatureObject);
}

//=============================================================================
