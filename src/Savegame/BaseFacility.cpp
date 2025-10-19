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
#include "BaseFacility.h"
#include "../Mod/RuleBaseFacility.h"
#include "../Engine/GraphSubset.h"
#include "Base.h"
#include "ItemContainer.h"

namespace OpenXcom
{

/**
 * Initializes a base facility of the specified type.
 * @param rules Pointer to ruleset.
 * @param base Pointer to base of origin.
 */
BaseFacility::BaseFacility(const RuleBaseFacility *rules, Base *base) :
	_rules(rules), _base(base),
	_x(-1), _y(-1), _buildTime(0),
	_ammo(0),
	_ammoMissingReported(false),
	_disabled(false),
	_craftsForDrawing(0),
	_hadPreviousFacility(false)
{
}

/**
 *
 */
BaseFacility::~BaseFacility()
{
}

/**
 * Loads the base facility from a YAML file.
 * @param node YAML node.
 */
void BaseFacility::load(const YAML::YamlNodeReader& reader)
{
	reader.tryRead("x", _x);
	reader.tryRead("y", _y);
	reader.tryRead("buildTime", _buildTime);
	reader.tryRead("ammo", _ammo);
	reader.tryRead("ammoMissingReported", _ammoMissingReported);
	reader.tryRead("disabled", _disabled);
	reader.tryRead("hadPreviousFacility", _hadPreviousFacility);
}

/**
 * Saves the base facility to a YAML file.
 * @return YAML node.
 */
void BaseFacility::save(YAML::YamlNodeWriter writer) const
{
	writer.setAsMap();
	writer.write("type", _rules->getType());
	writer.write("x", _x);
	writer.write("y", _y);
	if (_buildTime != 0)
		writer.write("buildTime", _buildTime);
	if (_ammo != 0)
		writer.write("ammo", _ammo);
	if (_ammoMissingReported)
		writer.write("ammoMissingReported", _ammoMissingReported);
	if (_disabled)
		writer.write("disabled", _disabled);
	if (_hadPreviousFacility)
		writer.write("hadPreviousFacility", _hadPreviousFacility);
}

/**
 * Returns the ruleset for the base facility's type.
 * @return Pointer to ruleset.
 */
const RuleBaseFacility *BaseFacility::getRules() const
{
	return _rules;
}

/**
 * Returns the base facility's X position on the
 * base grid that it's placed on.
 * @return X position in grid squares.
 */
int BaseFacility::getX() const
{
	return _x;
}

/**
 * Changes the base facility's X position on the
 * base grid that it's placed on.
 * @param x X position in grid squares.
 */
void BaseFacility::setX(int x)
{
	_x = x;
}

/**
 * Returns the base facility's Y position on the
 * base grid that it's placed on.
 * @return Y position in grid squares.
 */
int BaseFacility::getY() const
{
	return _y;
}

/**
 * Changes the base facility's Y position on the
 * base grid that it's placed on.
 * @param y Y position in grid squares.
 */
void BaseFacility::setY(int y)
{
	_y = y;
}

/**
 * Get placement of facility in base.
 */
BaseAreaSubset BaseFacility::getPlacement() const
{
	int sizeX = _rules->getSizeX();
	int sizeY = _rules->getSizeY();
	return BaseAreaSubset(sizeX, sizeY).offset(_x, _y);
}

/**
 * Returns the base facility's remaining time
 * until it's finished building (0 = complete).
 * @return Time left in days.
 */
int BaseFacility::getBuildTime() const
{
	return _buildTime;
}

/**
 * Returns the base facility's remaining time
 * until it's finished building (0 = complete).
 * Facility upgrades and downgrades are ignored in this calculation.
 * @return Time left in days.
 */
int BaseFacility::getAdjustedBuildTime() const
{
	if (_hadPreviousFacility)
		return 0;

	return _buildTime;
}

/**
 * Changes the base facility's remaining time
 * until it's finished building.
 * @param time Time left in days.
 */
void BaseFacility::setBuildTime(int time)
{
	_buildTime = time;
}

/**
 * Handles the facility building every day.
 */
void BaseFacility::build()
{
	_buildTime--;
	if (_buildTime == 0)
		_hadPreviousFacility = false;
}

/**
 * Returns if this facility is currently being
 * used by its base.
 * @return True if it's under use, False otherwise.
 */
BasePlacementErrors BaseFacility::inUse() const
{
	if (_buildTime > 0)
	{
		return BPE_None;
	}

	return _base->isAreaInUse(getPlacement());
}

/**
 * Rearms the facility.
 * @return The ammo item missing for rearming, or nullptr if none or reported already.
 */
const RuleItem* BaseFacility::rearm()
{
	// facility doesn't need to be rearmed at all (vanilla)
	if (_rules->getAmmoMax() <= 0)
		return nullptr;

	// facility is not operational
	if (_buildTime > 0)
		return nullptr;

	// facility is already fully armed
	if (_ammo >= _rules->getAmmoMax())
	{
		resetAmmoMissingReported();
		return nullptr;
	}

	int ammoMissing = _rules->getAmmoMax() - _ammo;
	int ammoUsed = std::min(ammoMissing, _rules->getRearmRate());

	const RuleItem* ammoItem = nullptr;
	if (_rules->getAmmoItem())
	{
		int ammoAvailable = _base->getStorageItems()->getItem(_rules->getAmmoItem());
		if (ammoAvailable < ammoUsed)
		{
			if (!_ammoMissingReported)
			{
				ammoItem = _rules->getAmmoItem();
				_ammoMissingReported = true;
			}
			ammoUsed = ammoAvailable;
		}
		_base->getStorageItems()->removeItem(_rules->getAmmoItem(), ammoUsed);
	}

	_ammo += ammoUsed;

	return ammoItem;
}

/**
 * Checks if the facility is disabled.
 * @return True if facility is disabled, False otherwise.
 */
bool BaseFacility::getDisabled() const
{
	return _disabled;
}

/**
 * Sets the facility's disabled flag.
 * @param disabled flag to set.
 */
void BaseFacility::setDisabled(bool disabled)
{
	_disabled = disabled;
}

/**
* Gets crafts vector, used for drawing facility.
 * @return crafts vector at the facility
 */
std::vector<Craft *> BaseFacility::getCraftsForDrawing()
{
	return _craftsForDrawing;
}

/**
 * Sets a vector of crafts, used for drawing facility.
 * @param vector of Crafts to copy to other facility, craftV
 */
 void BaseFacility::setCraftsForDrawing(std::vector<Craft*> craftV)
{
		_craftsForDrawing = craftV;
}

/**
 * Add another craft, used for drawing facility.
 * @param craft for drawing hangar.
 */
void BaseFacility::addCraftForDrawing(Craft *craft)
{
	_craftsForDrawing.push_back(craft);
}

/**
 * Delete an already included craft, used for drawing facility.
 * @param craft to delete
 */
std::vector<Craft*>::iterator BaseFacility::delCraftForDrawing(Craft *craft)
{
	std::vector<Craft*>::iterator c;
	for (c = _craftsForDrawing.begin(); c != _craftsForDrawing.end(); ++c)
	{
		if (*c == craft)
		{
			return _craftsForDrawing.erase(c);
		}
	}
	return c;
}

/**
 * Clear vector of crafts at the facility
 */
void BaseFacility::clearCraftsForDrawing()
{
	_craftsForDrawing.clear();
}

/**
 * Gets whether this facility was placed over another or was placed by removing another
 * @return true if placed over or by removing another facility
 */
bool BaseFacility::getIfHadPreviousFacility() const
{
	return _hadPreviousFacility;
}

/**
 * Sets whether this facility was placed over another or was placed by removing another
 * @param was there another facility just here?
 */
void BaseFacility::setIfHadPreviousFacility(bool hadPreviousFacility)
{
	_hadPreviousFacility = hadPreviousFacility;
}

/**
 * Is the facility fully built or being upgraded/downgraded?
 * Used for determining if this facility should count for base connectivity
 * @return True, if fully built or being upgraded/downgraded.
 */
bool BaseFacility::isBuiltOrHadPreviousFacility() const
{
	return _buildTime == 0 || _hadPreviousFacility;
}

}
