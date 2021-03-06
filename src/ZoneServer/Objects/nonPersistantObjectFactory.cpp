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

#include "ZoneServer/Objects/nonPersistantObjectFactory.h"
#include "CampRegion.h"
#include "CampTerminal.h"
//#include "DraftSchematic.h"
#include "ZoneServer/Objects/Instrument.h"
#include "ZoneServer/GameSystemManagers/Structure Manager/PlayerStructure.h"
#include "Zoneserver/Objects/Item.h"
#include "ItemFactory.h"
#include "ZoneServer\Objects\Object\ObjectManager.h"
#include "ZoneServer/Objects/Object/ObjectFactory.h"
#include "ZoneServer/Objects/Object/ObjectFactoryCallback.h"
#include "ZoneServer/Objects/Player Object/PlayerObject.h"
#include "ZoneServer/GameSystemManagers/Resource Manager/ResourceManager.h"
#include "ZoneServer/GameSystemManagers/Structure Manager/StructureManager.h"
#include "ZoneServer/WorldManager.h"


#include "MessageLib/MessageLib.h"

#include "DatabaseManager/Database.h"
#include "DatabaseManager/DatabaseResult.h"
#include "DatabaseManager/DataBinding.h"
#include "Utils/utils.h"
#include "Utils/typedefs.h"

#include <anh\app\swganh_kernel.h>
//=============================================================================

//bool							NonPersistantObjectFactory::mInsFlag    = false;
NonPersistantObjectFactory*		NonPersistantObjectFactory::mSingleton  = NULL;

//======================================================================================================================

NonPersistantObjectFactory* NonPersistantObjectFactory::Instance(void)
{
    if (!mSingleton)
    {
        mSingleton = new NonPersistantObjectFactory(gWorldManager->getKernel());
    }
    return mSingleton;
}

//=============================================================================

// This constructor prevents the default constructor to be used, as long as the constructor is kept private.

NonPersistantObjectFactory::NonPersistantObjectFactory() : FactoryBase(NULL)
{
}

//=============================================================================

NonPersistantObjectFactory::NonPersistantObjectFactory(swganh::app::SwganhKernel*	kernel) : FactoryBase(kernel)
{
 
#if defined(_MSC_VER)
    mId			= 0x0000100000000000;
#else
    mId			= 0x0000100000000000LLU;
#endif
    _setupDatabindings();
}

//=============================================================================

NonPersistantObjectFactory::~NonPersistantObjectFactory()
{
    _destroyDatabindings();

}

//=============================================================================

void NonPersistantObjectFactory::handleDatabaseJobComplete(void* ref,swganh::database::DatabaseResult* result)
{
    NonPersistantQueryContainerBase* asyncContainer = reinterpret_cast<NonPersistantQueryContainerBase*>(ref);

    switch(asyncContainer->mQueryType)
    {
    case NonPersistantItemFactoryQuery_MainData:
    {
        Item* item = asyncContainer->mItem;
        _createItem(result,item);

        if(item->getLoadState() == LoadState_Attributes)
        {
            QueryContainerBase* asContainer = new(mQueryContainerPool.ordered_malloc()) QueryContainerBase(asyncContainer->mOfCallback,ItemFactoryQuery_Attributes,asyncContainer->mClient);
            asContainer->mObject = item;

            mDatabase->executeSqlAsync(this,asContainer,"SELECT attributes.name,item_family_attribute_defaults.attribute_value,attributes.internal"
                                       " FROM %s.item_family_attribute_defaults"
                                       " INNER %s.JOIN attributes ON (item_family_attribute_defaults.attribute_id = attributes.id)"
                                       " WHERE item_family_attribute_defaults.item_type_id = %u ORDER BY item_family_attribute_defaults.attribute_order",
                                       mDatabase->galaxy(),mDatabase->galaxy(),item->getItemType());
           
        }
    }
    break;

    case ItemFactoryQuery_Attributes:
    {

        _buildAttributeMap(asyncContainer->mObject,result);

        if(asyncContainer->mObject->getLoadState() == LoadState_Loaded)
            asyncContainer->mOfCallback->handleObjectReady(asyncContainer->mObject,asyncContainer->mClient);

    }
    break;

    default:
        break;
    }

    mQueryContainerPool.free(asyncContainer);
}

//////////////////////////////////////////////////////////////////////////////////
//creates a non persistant tangible object
//loads the necessary blueprints from db
//////////////////////////////////////////////////////////////////////////////////
void NonPersistantObjectFactory::createTangible(ObjectFactoryCallback* ofCallback, uint32 familyId,uint32 typeId,uint64 parentId, const glm::vec3& position,BString customName,DispatchClient* client)
//gObjectFactory->requestNewDefaultItem
//(this,11,1320,entertainer->getId(),99,Anh_Math::Vector3(),"");
{
    //items.maxCondition,items.damage,items.dynamicint32 "
    //get blueprint out of the db
    Item* newItem;
    if (familyId == ItemFamily_Instrument)
    {
        newItem = new (Instrument);
    }
    else
    {
        newItem = new (Item);
    }
    newItem->mPosition = position;
    newItem->setItemFamily(familyId);
    newItem->setItemType(typeId);
    newItem->setParentId(parentId);
    std::string name = customName.getAnsi();
	newItem->setCustomName(std::u16string(name.begin(), name.end()));
    newItem->setMaxCondition(100);
    newItem->setTangibleGroup(TanGroup_Item);
    newItem->setId(gWorldManager->getRandomNpId());

    int8 sql[256];
    sprintf(sql,"SELECT item_types.object_string,item_types.stf_name,item_types.stf_file,item_types.stf_detail_name, item_types.stf_detail_file FROM %s.item_types WHERE item_types.id = '%u'",mDatabase->galaxy(),typeId);
    mDatabase->executeSqlAsync(this,new(mQueryContainerPool.ordered_malloc()) NonPersistantQueryContainerBase(ofCallback,NonPersistantItemFactoryQuery_MainData,client,newItem),sql);
    

    //asyncContainer->mOfCallback->handleObjectReady(item,asyncContainer->mClient);
}

/////////////////////////////////////////////////////////////////////////////////
//we create a camp at the given coordinates together with all the necessary additions as found in structure_item_template
//

TangibleObject* NonPersistantObjectFactory::spawnTangible(StructureItemTemplate* placableTemplate, uint64 parentId, const glm::vec3& position, const std::string customName, PlayerObject* player)
{
    	
	//we dont set the types here as we are factually placing statics / and or items / terminals
	//but we need to revisit this when dealing with high level camps
	//in which case we have to do separate create functions for terminals crafting stations and so on
	TangibleObject* tangible = new(TangibleObject);
	
	tangible->mPosition = position;
	tangible->mPosition.x += placableTemplate->mPosition.x;
	tangible->mPosition.y += placableTemplate->mPosition.y;
	tangible->mPosition.z += placableTemplate->mPosition.z;
	
	tangible->mDirection.x = placableTemplate->mDirection.x;
	tangible->mDirection.y = placableTemplate->mDirection.y;
	tangible->mDirection.z = placableTemplate->mDirection.z;

	tangible->mDirection.w = placableTemplate->mDirection.w;

	tangible->setName(placableTemplate->name.getAnsi());
	tangible->setNameFile(placableTemplate->file.getAnsi());

	tangible->setParentId(parentId);
	tangible->setCustomName(std::u16string(customName.begin(), customName.end()));
	tangible->setMaxCondition(100);

	tangible->setTangibleGroup(placableTemplate->tanType);
	
	//see above notes
	tangible->setTangibleType(TanType_None);

	tangible->setId(gWorldManager->getRandomNpId());

	tangible->SetTemplate(placableTemplate->structureObjectString.getAnsi());

	//create it in the world
	tangible->mDirection = player->mDirection;
	//tangible->setTypeOptions(0xffffffff);

	gObjectManager->LoadSlotsForObject(tangible);

	gWorldManager->addObject(tangible);			

	//add to the si
	gSpatialIndexManager->createInWorld(tangible);
	
	gMessageLib->sendDataTransform053(tangible);

	return(tangible);

}

CampTerminal* NonPersistantObjectFactory::spawnTerminal(StructureItemTemplate* placableTemplate, uint64 parentId, const glm::vec3& position, const std::string customName, PlayerObject* player, StructureDeedLink*	deedData)
{	
	//we dont set the types here as we are factually placing statics / and or items / terminals
	//but we need to revisit this when dealing with high level camps
	//in which case we have to do separate create functions for terminals crafting stations and so on
	CampTerminal* terminal = new(CampTerminal);
	
	terminal->mPosition = position;
	terminal->mPosition.x += placableTemplate->mPosition.x;
	terminal->mPosition.y += placableTemplate->mPosition.y;
	terminal->mPosition.z += placableTemplate->mPosition.z;

	terminal->mDirection.x = placableTemplate->mDirection.x;
	terminal->mDirection.y = placableTemplate->mDirection.y;
	terminal->mDirection.z = placableTemplate->mDirection.z;

	terminal->mDirection.w = placableTemplate->mDirection.w;

	terminal->setName(deedData->stf_name.getAnsi());
	terminal->setNameFile(deedData->stf_file.getAnsi());

	
	terminal->setParentId(parentId);
	terminal->setCustomName(std::u16string(customName.begin(), customName.end()));
	terminal->setMaxCondition(100);

	terminal->setTangibleGroup(placableTemplate->tanType);
	
	//see above notes
	terminal->setTangibleType(TanType_CampTerminal);

	terminal->setId(gWorldManager->getRandomNpId());

	//tangible->setOwner(player->getId());

	terminal->SetTemplate(placableTemplate->structureObjectString.getAnsi());

	//create it in the world
	//tangible->mDirection = player->mDirection;
	
	gWorldManager->addObject(terminal);

	//add to the si
	gSpatialIndexManager->createInWorld(terminal);

	return(terminal);
}


/////////////////////////////////////////////////////////////////////////////////
//we create a camp at the given coordinates together with all the necessary additions as found in structure_item_template
//


void NonPersistantObjectFactory::_createItem(swganh::database::DatabaseResult* result,Item* item)
{
    if (!result->getRowCount()) {
    	return;
    }

    result->getNextRow(mItemBinding,item);

    item->setLoadState(LoadState_Attributes);
}


void NonPersistantObjectFactory::_setupDatabindings()
{

    mItemBinding = mDatabase->createDataBinding(4);

	mItemBinding->addField(swganh::database::DFT_stdstring,offsetof(Item,template_string_),256,0);
    mItemBinding->addField(swganh::database::DFT_bstring,offsetof(Item,mName),64,1);
    mItemBinding->addField(swganh::database::DFT_bstring,offsetof(Item,mNameFile),64,2);
    mItemBinding->addField(swganh::database::DFT_bstring,offsetof(Item,mDetailFile),64,4);

}

void NonPersistantObjectFactory::_destroyDatabindings()
{
    mDatabase->destroyDataBinding(mItemBinding);

}

void NonPersistantObjectFactory::requestObject(ObjectFactoryCallback* ofCallback,uint64 id,uint16 subGroup,uint16 subType,DispatchClient* client)
{
}

PlayerStructure* NonPersistantObjectFactory::requestBuildingFenceObject(float x, float y, float z, PlayerObject* player)
{

    PlayerStructure* structure = new(PlayerStructure);

    structure->setType(ObjType_Structure);
    structure->mPosition.x = x;
    structure->mPosition.z = z;
    //slow query - use for building placement only
    structure->mPosition.y = y;

    structure->setName("temporary_structure");
    structure->setNameFile("player_structure");


    structure->setParentId(0);

    structure->setMaxCondition(1000);

    structure->setPlayerStructureFamily(PlayerStructure_Fence);

    structure->setId(gWorldManager->getRandomNpId());

    //tangible->setOwner(player->getId());

    //create it in the world
	structure->SetTemplate("object/installation/base/shared_construction_installation_base.iff");
	gObjectManager->LoadSlotsForObject(structure);

	gWorldManager->addObject(structure);

	//add to the si
	gSpatialIndexManager->createInWorld(structure);
		
	gMessageLib->sendDataTransform053(structure);

    return structure;

}

PlayerStructure* NonPersistantObjectFactory::requestBuildingSignObject(float x, float y, float z, PlayerObject* player, BString name, BString namefile, std::string custom)
{

    PlayerStructure* structure = new(PlayerStructure);

    structure->setType(ObjType_Structure);
    structure->mPosition.x = x;
    structure->mPosition.z = z;
    structure->mPosition.y = y;

    structure->setName(name.getAnsi());
    structure->setNameFile(namefile.getAnsi());
    structure->setCustomName(std::u16string(custom.begin(), custom.end()));

    structure->setParentId(0);
    structure->setCustomName(std::u16string());
    structure->setMaxCondition(1000);

    structure->setPlayerStructureFamily(PlayerStructure_Sign);

    structure->setId(gWorldManager->getRandomNpId());

    //tangible->setOwner(player->getId());

    structure->SetTemplate("object/static/structure/tatooine/shared_streetsign_wall_style_01.iff");
	gObjectManager->LoadSlotsForObject(structure);
	//create it in the world
	
	gWorldManager->addObject(structure);

	//add to the si
	gSpatialIndexManager->createInWorld(structure);
		
	gMessageLib->sendDataTransform053(structure);

    return structure;

}

//================================================================================================================
//
// we clone a nonpersistant tangible here - we use it when we get an item out of a crate while crafting for example
//
TangibleObject* NonPersistantObjectFactory::cloneTangible(TangibleObject* theTemplate)
{

    TangibleObject* tangible = new(TangibleObject);

    tangible->setId(gWorldManager->getRandomNpId());

    tangible->mPosition = theTemplate->mPosition;
    tangible->mDirection = theTemplate->mDirection;

    tangible->setName(theTemplate->getName().getAnsi());
    tangible->setNameFile(theTemplate->getNameFile().getAnsi());

    tangible->setParentId(theTemplate->getParentId());
    tangible->setCustomName(theTemplate->getCustomName());
    tangible->setMaxCondition(theTemplate->getMaxCondition());

    tangible->setTangibleGroup(theTemplate->getTangibleGroup());

    //see above notes
    tangible->setTangibleType(theTemplate->getTangibleType());



    tangible->SetTemplate(theTemplate->GetTemplate());

    return(tangible);

}



