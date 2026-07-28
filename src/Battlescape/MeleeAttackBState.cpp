/*
 * Copyright 2010-2016 OpenXcom Developers.
 *
 * This file is part of OpenXcom.
 *
 * OpenXcom is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OpenXcom is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenXcom.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "MeleeAttackBState.h"
#include "ExplosionBState.h"
#include "BattlescapeGame.h"
#include "BattlescapeState.h"
#include "TileEngine.h"
#include "Map.h"
#include "Camera.h"
#include "AIModule.h"
#include "../Savegame/Tile.h"
#include "../Savegame/SavedBattleGame.h"
#include "../Savegame/BattleUnit.h"
#include "../Savegame/BattleItem.h"
#include "../Engine/Exception.h"
#include "../Mod/Mod.h"
#include "../Mod/RuleItem.h"
#include "../fmath.h"

namespace OpenXcom
{

/**
 * Sets up a MeleeAttackBState.
 */
MeleeAttackBState::MeleeAttackBState(BattlescapeGame *parent, BattleAction action) : BattleState(parent, action), _unit(0), _target(0), _weapon(0), _ammo(0), _hitNumber(0), _initialized(false), _reaction(false)
{
}

/**
 * Deletes the MeleeAttackBState.
 */
MeleeAttackBState::~MeleeAttackBState()
{
}

/**
 * Initializes the sequence.
 * does a lot of validity checking.
 */
void MeleeAttackBState::init()
{
	if (_initialized) return;
	_initialized = true;

	int terrainMeleeTilePart = _action.terrainMeleeTilePart;
	_action.terrainMeleeTilePart = 0; // reset!

	_weapon = _action.weapon;
	if (!_weapon) // can't hit without weapon
	{
		_parent->popState();
		return;
	}

	_unit = _action.actor;

	bool reactionShoot = _unit->getFaction() != _parent->getSave()->getSide();
	_ammo = _action.weapon->getAmmoForAction(BA_HIT, reactionShoot ? nullptr : &_action.result);
	if (!_ammo)
	{
		_parent->popState();
		return;
	}

	if (!_parent->getSave()->getTile(_action.target)) // invalid target position
	{
		_parent->popState();
		return;
	}

	if (_unit->isOut() || _unit->isOutThresholdExceed())
	{
		// something went wrong - we can't shoot when dead or unconscious, or if we're about to fall over.
		_parent->popState();
		return;
	}

	// reaction fire
	if (reactionShoot)
	{
		// no ammo or target is dead: give the time units back and cancel the shot.
		auto target = _parent->getSave()->getTile(_action.target)->getUnit();
		if (!target || target->isOut() || target->isOutThresholdExceed() || target != _parent->getSave()->getSelectedUnit())
		{
			_parent->popState();
			return;
		}
		_unit->lookAt(_action.target, _unit->getTurretType() != -1);
		while (_unit->getStatus() == STATUS_TURNING)
		{
			_unit->turn(_unit->getTurretType() != -1);
		}
	}
	
	// terrain melee
	if (terrainMeleeTilePart > 0)
	{
		_voxel = _action.target.toVoxel() + Position(8, 8, 12);

		if (terrainMeleeTilePart == 4)
		{ // Terrain melee higher floor object hit hack & helper
			while (_voxel.z > _action.target.toVoxel().z && _parent->getSave()->getTileEngine()->voxelCheck(_voxel, _unit) == V_EMPTY)
			{
				--_voxel.z;
			}
		}
		performMeleeAttack(terrainMeleeTilePart != 4 ? terrainMeleeTilePart : 0);
		return;
	}

	AIModule *ai = _unit->getAIModule();

	if (_unit->getFaction() == _parent->getSave()->getSide() &&
		_unit->getFaction() != FACTION_PLAYER &&
		_parent->_debugPlay == false &&
		ai && ai->getTarget())
	{
		_target = ai->getTarget();
	}
	else
	{
		_target = _parent->getSave()->getTile(_action.target)->getUnit();
	}

	if (!_target)
	{
		throw Exception("This is a known (but tricky) bug... still fixing it, sorry. In the meantime, try save scumming option or kill all aliens in debug mode to finish the mission.");
	}

	int height = _target->getFloatHeight() + (_target->getHeight() / 2) - _parent->getSave()->getTile(_action.target)->getTerrainLevel(_target);
	_voxel = _action.target.toVoxel() + Position(8, 8, height);

	/********************************\
	* -=ForcedMeleeToFloor=- section *
	\********************************/

	// pWWWa: adjust height coordinates (todo: make more unified)
	if (_parent->getSave()->isCtrlPressed(true) && _parent->getSave()->getSide() == FACTION_PLAYER && _unit->getFaction() == FACTION_PLAYER && !_unit->getTile()->hasNoFloor())
	{
		// Check presence of any alive unit under feet and apply their height (it is 0 usually, but let check)
		if (_target->getTile()->getTopItem() && _target->getTile()->getTopItem()->getUnit() && _target->getTile()->getTopItem()->getUnit()->getStatus() == STATUS_UNCONSCIOUS)
		{ 
			_target = _target->getTile()->getTopItem()->getUnit();
			_voxel.z = _target->getPosition().toVoxel().z;
		}
		else if (Mod::EXTENDED_TERRAIN_MELEE <= 0 ||
			     _action.weapon &&
			    (!_action.weapon->getRules()->getDamageType()->ToTile || !_action.weapon->getRules()->getMeleeType()->ToTile))
		{
			// Do not allow to dig terrain without activated Terrain Melee feature or for not suitable items for this
			_action.result = "STR_THERE_IS_NO_ONE_THERE";
			_parent->getCurrentAction()->type = BA_NONE; // stucking cursor fix, hello to BattleScapeGame
			_parent->popState();
			return;
		}
		else
		{
			// Let dive in & check till any terrain stuff will be found 
			while (_voxel.z > _action.target.toVoxel().z && _parent->getTileEngine()->voxelCheck(_voxel, _unit) == V_EMPTY)
			{
				--_voxel.z;
			}
		}
		goto endForceMeleeFloor;
	}

	// Miss unit, really ? Recheck tile's vertical axis
	if (_parent->getTileEngine()->voxelCheck(_voxel, _unit) != V_UNIT)
	{ 
		for (int z = 24; z >= 0; --z)
		{
			if (_parent->getTileEngine()->voxelCheck(_action.target.toVoxel() + Position(8, 8, z), _unit) == V_UNIT)
			{
				_voxel = _action.target.toVoxel() + Position(8, 8, z); break;
			}
		}
	}

	endForceMeleeFloor:

	if (!_parent->getSave()->getTile(_voxel.toTile()))
	{
		throw Exception("Melee attack animation overflow: target voxel is outside of the map boundaries.");
	}

	if (_unit->getFaction() == FACTION_HOSTILE)
	{
		_hitNumber = _weapon->getRules()->getAIMeleeHitCount() - 1;
	}

	performMeleeAttack();
}

/**
 * Performs all the overall functions of the state, this code runs AFTER the explosion state pops.
 */
void MeleeAttackBState::think()
{
	_parent->getSave()->getBattleState()->clearMouseScrollingState();
	if (_reaction && !_parent->getSave()->getUnitsFalling())
	{
		_reaction = false;
		if (_parent->getTileEngine()->checkReactionFire(_unit, _action))
		{
			return;
		}
	}

	// if the unit burns floor tiles, burn floor tiles
	if (_unit->getSpecialAbility() == SPECAB_BURNFLOOR || _unit->getSpecialAbility() == SPECAB_BURN_AND_EXPLODE)
	{
		_parent->getSave()->getTile(_action.target)->ignite(15);
	}
	if (_hitNumber > 0 &&
		// not performing a reaction attack
		_unit->getFaction() == _parent->getSave()->getSide() &&
		// whose target is still alive or at least conscious
		_target && !_target->isOutThresholdExceed() &&
		// and we still have ammo to make the attack
		_weapon->getAmmoForAction(BA_HIT))
	{
		--_hitNumber;
		performMeleeAttack();
	}
	else
	{
		if (_action.cameraPosition.z != -1)
		{
			_parent->getMap()->getCamera()->setMapOffset(_action.cameraPosition);
			_parent->getMap()->invalidate();
		}

		if (_unit->getFaction() == _parent->getSave()->getSide()) // not a reaction attack
		{
			_parent->getCurrentAction()->type = BA_NONE; // do this to restore cursor
		}

		if (_parent->getSave()->getSide() == FACTION_PLAYER || _parent->getSave()->getDebugMode())
		{
			_parent->setupCursor();
		}
		_parent->convertInfected();
		_parent->popState();
	}
}

/**
 * Sets up a melee attack, inserts an explosion into the map and make noises.
 */
void MeleeAttackBState::performMeleeAttack(int terrainMeleeTilePart)
{
	// sdend TU and go on or get out here
	if (!_action.spendTU(&_action.result)) return;
		
	// set the soldier in an aiming position
	_unit->aim(true);

	// use up ammo if applicable
	_action.weapon->spendAmmoForAction(BA_HIT, _parent->getSave());
	_parent->getMap()->setCursorType(CT_NONE);

	// offset the damage voxel ever so slightly so that the target knows which side the attack came from
	int attackerHeight = -_parent->getSave()->getTile(_unit->getPosition())->getTerrainLevel(_unit) + _unit->getHeight() / 2;
	Position attackerPos = _unit->getPositionVexels() + Position(0, 0, attackerHeight);
	Position difference = !_target ? _unit->getPosition() - _action.target : attackerPos - _voxel;

	// clamp the values for further "one step shifting" closer to attacker
	difference.x = Clamp<Sint16>(difference.x, -1, 1);
	difference.y = Clamp<Sint16>(difference.y, -1, 1);
	difference.z = Clamp<Sint16>(difference.z, -1, 1);

	// pWWWa: shift the impact position in victim's voxel space closer to the attacker's location
	while ( _parent->getTileEngine()->voxelCheck((_voxel + difference), _unit) == V_UNIT &&
		   (_voxel.x != attackerPos.x || _voxel.y != attackerPos.y || _voxel.z != attackerPos.z) )
	{
		_voxel += difference;
	}

	// make an explosion action
	_parent->statePushFront(new ExplosionBState(_parent, _voxel, BattleActionAttack::GetAferShoot(_action, _ammo), 0, true, 0, 0, terrainMeleeTilePart));


	_reaction = true;
}

}
