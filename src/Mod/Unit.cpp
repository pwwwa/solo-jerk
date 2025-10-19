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
#include "Unit.h"
#include "../Engine/Exception.h"
#include "../Engine/ScriptBind.h"
#include "LoadYaml.h"
#include "Mod.h"
#include "Armor.h"

namespace OpenXcom
{

/**
 * Creates a certain type of unit.
 * @param type String defining the type.
 */
Unit::Unit(const std::string &type) :
	_type(type), _liveAlienName(Mod::STR_NULL), _showFullNameInAlienInventory(-1), _armor(nullptr), _standHeight(0), _kneelHeight(0), _floatHeight(0), _value(0),
	_moraleLossWhenKilled(100), _moveSound(-1), _intelligence(0), _aggression(0),
	_spotter(0), _sniper(0), _energyRecovery(30), _specab(SPECAB_NONE), _livingWeapon(false), _aiTargetMode(0),
	_psiWeapon("ALIEN_PSI_WEAPON"), _capturable(true), _canSurrender(false), _autoSurrender(false),
	_isLeeroyJenkins(false), _waitIfOutsideWeaponRange(false), _pickUpWeaponsMoreActively(-1), _avoidsFire(defBoolNullable),
	_isBrutal(false), _isNotBrutal(false), _isCheatOnMovement(false), _isAggressive(false),
	_vip(false), _cosmetic(false), _ignoredByAI(false),
	_canPanic(true), _canBeMindControlled(true), _berserkChance(33)
{
}

/**
 *
 */
Unit::~Unit()
{
	for (auto* opts : _weightedBuiltInWeapons)
	{
		delete opts;
	}
}

/**
 * Loads the unit from a YAML file.
 * @param node YAML node.
 * @param mod Mod for the unit.
 */
void Unit::load(const YAML::YamlNodeReader& node, Mod* mod)
{
    const auto& reader = node.useIndex();

    // Handle parent node if exists
    if (const auto& parent = reader["refNode"])
    {
        load(parent, mod);
    }

    // Load various properties
    mod->loadNameNull(_type, _civilianRecoveryTypeName, reader["civilianRecoveryType"]);
    mod->loadNameNull(_type, _spawnedPersonName, reader["spawnedPersonName"]);
    mod->loadNameNull(_type, _liveAlienName, reader["liveAlien"]);

    if (reader["spawnedSoldier"])
    {
        _spawnedSoldier = reader["spawnedSoldier"].emitDescendants(YAML::YamlRootNodeReader(_spawnedSoldier, "(spawned soldier template)"));
    }

    // Standard properties
    reader.tryRead("race", _race);
    reader.tryRead("showFullNameInAlienInventory", _showFullNameInAlienInventory);
    reader.tryRead("rank", _rank);
    _stats.merge(reader["stats"].readVal(_stats));
    mod->loadName(_type, _armorName, reader["armor"]);
    reader.tryRead("standHeight", _standHeight);
    reader.tryRead("kneelHeight", _kneelHeight);
    reader.tryRead("floatHeight", _floatHeight);
    if (_floatHeight + _standHeight > 25)
    {
        throw Exception("Error with unit " + _type + ": Unit height may not exceed 25");
    }
    reader.tryRead("value", _value);
    reader.tryRead("moraleLossWhenKilled", _moraleLossWhenKilled);
    reader.tryRead("intelligence", _intelligence);
    reader.tryRead("aggression", _aggression);
    reader.tryRead("spotter", _spotter);
    reader.tryRead("sniper", _sniper);
    reader.tryRead("energyRecovery", _energyRecovery);
    reader.tryRead("specab", _specab);
    reader.tryRead("spawnUnit", _spawnUnitName);
    reader.tryRead("livingWeapon", _livingWeapon);
    reader.tryRead("canSurrender", _canSurrender);
    reader.tryRead("autoSurrender", _autoSurrender);
    reader.tryRead("isLeeroyJenkins", _isLeeroyJenkins);

    // Custom additions
    reader.tryRead("isBrutal", _isBrutal);
    reader.tryRead("isNotBrutal", _isNotBrutal);
    reader.tryRead("isCheatOnMovement", _isCheatOnMovement);

    reader.tryRead("waitIfOutsideWeaponRange", _waitIfOutsideWeaponRange);
    reader.tryRead("pickUpWeaponsMoreActively", _pickUpWeaponsMoreActively);
    loadBoolNullable(_avoidsFire, reader["avoidsFire"]);
    reader.tryRead("meleeWeapon", _meleeWeapon);
    reader.tryRead("psiWeapon", _psiWeapon);
    reader.tryRead("capturable", _capturable);
    reader.tryRead("vip", _vip);
    reader.tryRead("cosmetic", _cosmetic);
    reader.tryRead("ignoredByAI", _ignoredByAI);
    reader.tryRead("canPanic", _canPanic);
    reader.tryRead("canBeMindControlled", _canBeMindControlled);
    reader.tryRead("berserkChance", _berserkChance);
    reader.tryRead("builtInWeaponSets", _builtInWeaponsNames);

    if (reader["builtInWeapons"])
    {
        _builtInWeaponsNames.push_back(reader["builtInWeapons"].readVal<std::vector<std::string>>());
    }

    for (const auto& weights : reader["weightedBuiltInWeaponSets"].children())
    {
        WeightedOptions* nw = new WeightedOptions();
        nw->load(weights);
        _weightedBuiltInWeapons.push_back(nw);
    }

    // Load sound offsets
    mod->loadSoundOffset(_type, _deathSound, reader["deathSound"], "BATTLE.CAT");
    mod->loadSoundOffset(_type, _panicSound, reader["panicSound"], "BATTLE.CAT");
    mod->loadSoundOffset(_type, _berserkSound, reader["berserkSound"], "BATTLE.CAT");
    mod->loadSoundOffset(_type, _aggroSound, reader["aggroSound"], "BATTLE.CAT");

    mod->loadSoundOffset(_type, _selectUnitSound, reader["selectUnitSound"], "BATTLE.CAT");
    mod->loadSoundOffset(_type, _startMovingSound, reader["startMovingSound"], "BATTLE.CAT");
    mod->loadSoundOffset(_type, _selectWeaponSound, reader["selectWeaponSound"], "BATTLE.CAT");
    mod->loadSoundOffset(_type, _annoyedSound, reader["annoyedSound"], "BATTLE.CAT");

    mod->loadSoundOffset(_type, _moveSound, reader["moveSound"], "BATTLE.CAT");
}

/**
 * Cross link with other rules
 */
void Unit::afterLoad(const Mod* mod)
{
	mod->linkRule(_armor, _armorName);
	mod->linkRule(_spawnUnit, _spawnUnitName);
	mod->linkRule(_builtInWeapons, _builtInWeaponsNames);
	if (_liveAlienName == Mod::STR_NULL)
	{
		_liveAlien = mod->getItem(_type, false); // this is optional default behavior
	}
	else
	{
		mod->linkRule(_liveAlien, _liveAlienName);
	}

	if (Mod::isEmptyRuleName(_civilianRecoveryTypeName) == false)
	{
		if (!isRecoverableAsEngineer() && !isRecoverableAsScientist())
		{
			_civilianRecoverySoldierType = mod->getSoldier(_civilianRecoveryTypeName, false);
			if (_civilianRecoverySoldierType)
			{
				_civilianRecoveryTypeName = "";
			}
			else
			{
				mod->linkRule(_civilianRecoveryItemType, _civilianRecoveryTypeName);
			}
		}
		assert(isRecoverableAsCivilian() && "Check missing some cases");
	}

	mod->checkForSoftError(_armor == nullptr, _type, "Unit is missing armor", LOG_ERROR);
	if (_armor)
	{
		if (_capturable && _armor->getCorpseBattlescape().front()->isRecoverable() && _spawnUnit == nullptr)
		{
			mod->checkForSoftError(
				_liveAlien == nullptr && !isRecoverableAsCivilian(),
				_type,
				"This unit can be recovered (in theory), but there is no corresponding 'liveAlien:' or 'civilianRecoveryType:' to recover.",
				LOG_INFO
			);
		}
		else
		{
			std::string s =
				!_capturable ? "the unit is marked with 'capturable: false'" :
				!_armor->getCorpseBattlescape().front()->isRecoverable() ? "the first 'corpseBattle' item of the unit's armor is marked with 'recover: false'" :
				_spawnUnit != nullptr ? "the unit will be converted into another unit type on stun/kill/capture" :
				"???";

			mod->checkForSoftError(
				_liveAlien
				&& _liveAlien->getVehicleUnit() == nullptr
				&& _spawnUnit == nullptr, // if unit is `_capturable` we can still get live species even if it can spawn unit
				_type,
				"This unit has a corresponding item to recover, but still isn't recoverable. Reason: (" + s + "). Consider marking the unit with 'liveAlien: \"\"'.",
				LOG_INFO
			);
		}
	}
}

/**
 * Returns the language string that names
 * this unit. Each unit type has a unique name.
 * @return The unit's name.
 */
const std::string& Unit::getType() const
{
	return _type;
}

/**
 * Returns the unit's stats data object.
 * @return The unit's stats.
 */
UnitStats *Unit::getStats()
{
	return &_stats;
}

/**
 * Returns the unit's height at standing.
 * @return The unit's height.
 */
int Unit::getStandHeight() const
{
	return _standHeight;
}

/**
 * Returns the unit's height at kneeling.
 * @return The unit's kneeling height.
 */
int Unit::getKneelHeight() const
{
	return _kneelHeight;
}

/**
 * Returns the unit's floating elevation.
 * @return The unit's floating height.
 */
int Unit::getFloatHeight() const
{
	return _floatHeight;
}

/**
 * Gets the unit's armor type.
 * @return The unit's armor type.
 */
Armor* Unit::getArmor() const
{
	return const_cast<Armor*>(_armor); //TODO: fix this function usage to remove const cast
}

/**
 * Gets the alien's race.
 * @return The alien's race.
 */
std::string Unit::getRace() const
{
	return _race;
}

/**
 * Gets the unit's rank.
 * @return The unit's rank.
 */
std::string Unit::getRank() const
{
	return _rank;
}

/**
 * Gets the unit's value - for scoring.
 * @return The unit's value.
 */
int Unit::getValue() const
{
	return _value;
}

/**
* Get the unit's death sounds.
* @return List of sound IDs.
*/
const std::vector<int> &Unit::getDeathSounds() const
{
	return _deathSound;
}

/**
 * Gets the unit's panic sounds.
 * @return List of sound IDs.
 */
const std::vector<int> &Unit::getPanicSounds() const
{
	return _panicSound;
}

/**
 * Gets the unit's berserk sounds.
 * @return List of sound IDs.
 */
const std::vector<int> &Unit::getBerserkSounds() const
{
	return _berserkSound;
}

/**
 * Gets the unit's move sound.
 * @return The id of the unit's move sound.
 */
int Unit::getMoveSound() const
{
	return _moveSound;
}

/**
 * Gets the intelligence. This is the number of turns the AI remembers your troop positions.
 * @return The unit's intelligence.
 */
int Unit::getIntelligence() const
{
	return _intelligence;
}

/**
 * Gets the aggression. Determines the chance of revenge and taking cover.
 * @return The unit's aggression.
 */
int Unit::getAggression() const
{
	return _aggression;
}

/**
 * Gets the spotter score. Determines how many turns sniper AI units can act on this unit seeing your troops.
 * @return The unit's spotter value.
 */
int Unit::getSpotterDuration() const
{
	// Lazy balance - use -1 to make this the same as intelligence value
	return (_spotter == -1) ? _intelligence : _spotter;
}

/**
 * Gets the sniper score. Determines the chances of firing from out of LOS on spotted units.
 * @return The unit's sniper value.
 */
int Unit::getSniperPercentage() const
{
	return _sniper;
}

/**
 * Gets the unit's special ability.
 * @return The unit's specab.
 */
int Unit::getSpecialAbility() const
{
	return (int)_specab;
}

/**
 * Gets the unit that is spawned when this one dies.
 * @return The unit's spawn unit.
 */
const Unit *Unit::getSpawnUnit() const
{
	return _spawnUnit;
}

/**
 * Gets the unit's aggro sounds (warcries).
 * @return List of sound IDs.
 */
const std::vector<int> &Unit::getAggroSounds() const
{
	return _aggroSound;
}

/**
 * How much energy does this unit recover per turn?
 * @return energy recovery amount.
 */
int Unit::getEnergyRecovery() const
{
	return _energyRecovery;
}

/**
 * Checks if this unit is a living weapon.
 * a living weapon ignores any loadout that may be available to
 * its rank and uses the one associated with its race.
 * @return True if this unit is a living weapon.
 */
bool Unit::isLivingWeapon() const
{
	return _livingWeapon;
}

/**
 * What is this unit's built in melee weapon (if any).
 * @return the name of the weapon.
 */
const std::string &Unit::getMeleeWeapon() const
{
	return _meleeWeapon;
}

/**
* What is this unit's built in psi weapon (if any).
* @return the name of the weapon.
*/
const std::string &Unit::getPsiWeapon() const
{
	return _psiWeapon;
}

/**
 * What weapons does this unit have built in?
 * this is a vector of strings representing any
 * weapons that may be inherent to this creature.
 * note: unlike "livingWeapon" this is used in ADDITION to
 * any loadout or living weapon item that may be defined.
 * @return list of weapons that are integral to this unit.
 */
const std::vector<std::vector<const RuleItem*> > &Unit::getBuiltInWeapons() const
{
	return _builtInWeapons;
}

/**
* Gets whether the alien can be captured alive.
* @return a value determining whether the alien can be captured alive.
*/
bool Unit::getCapturable() const
{
	return _capturable;
}

/**
* Checks if this unit can surrender.
* @return True if this unit can surrender.
*/
bool Unit::canSurrender() const
{
	return _canSurrender || _autoSurrender;
}

/**
* Checks if this unit surrenders automatically, if all other units surrendered too.
* @return True if this unit auto-surrenders.
*/
bool Unit::autoSurrender() const
{
	return _autoSurrender;
}

/**
 * Is the unit afraid to pathfind through fire?
 * @return True if this unit has a penalty when pathfinding through fire.
 */
bool Unit::avoidsFire() const
{
	return useBoolNullable(_avoidsFire, _specab < SPECAB_BURNFLOOR);
}

/**
 * Should alien inventory show full name (e.g. Sectoid Leader) or just the race (e.g. Sectoid)?
 * @return True if full name can be shown.
 */
bool Unit::getShowFullNameInAlienInventory(Mod *mod) const
{
	if (_showFullNameInAlienInventory != -1)
	{
		return _showFullNameInAlienInventory == 0 ? false : true;
	}
	return mod->getShowFullNameInAlienInventory();
}


////////////////////////////////////////////////////////////
//					Script binding
////////////////////////////////////////////////////////////

namespace
{

void getTypeScript(const Unit* r, ScriptText& txt)
{
	if (r)
	{
		txt = { r->getType().c_str() };
		return;
	}
	else
	{
		txt = ScriptText::empty;
	}
}

std::string debugDisplayScript(const Unit* unit)
{
	if (unit)
	{
		std::string s;
		s += Unit::ScriptName;
		s += "(name: \"";
		s += unit->getType();
		s += "\")";
		return s;
	}
	else
	{
		return "null";
	}
}

} // namespace

void Unit::ScriptRegister(ScriptParserBase* parser)
{
	Bind<Unit> un = { parser };

	un.add<&getTypeScript>("getType");

	un.addDebugDisplay<&debugDisplayScript>();
}
/**
 * Register StatAdjustment in script parser.
 * @param parser Script parser.
 */
void StatAdjustment::ScriptRegister(ScriptParserBase* parser)
{
	Bind<StatAdjustment> sa = { parser };

	UnitStats::addGetStatsScript<&StatAdjustment::statGrowth>(sa, "");
}

// helper overloads for (de)serialization
bool read(ryml::ConstNodeRef const& n, UnitStats* val)
{
	YAML::YamlNodeReader reader(n);
	reader.tryRead("tu", val->tu);
	reader.tryRead("stamina", val->stamina);
	reader.tryRead("health", val->health);
	reader.tryRead("bravery", val->bravery);
	reader.tryRead("reactions", val->reactions);
	reader.tryRead("firing", val->firing);
	reader.tryRead("throwing", val->throwing);
	reader.tryRead("strength", val->strength);
	reader.tryRead("psiStrength", val->psiStrength);
	reader.tryRead("psiSkill", val->psiSkill);
	reader.tryRead("melee", val->melee);
	reader.tryRead("mana", val->mana);
	return true;
}

void write(ryml::NodeRef* n, UnitStats const& val)
{
	YAML::YamlNodeWriter writer(*n);
	writer.setAsMap();
	writer.write("tu", val.tu);
	writer.write("stamina", val.stamina);
	writer.write("health", val.health);
	writer.write("bravery", val.bravery);
	writer.write("reactions", val.reactions);
	writer.write("firing", val.firing);
	writer.write("throwing", val.throwing);
	writer.write("strength", val.strength);
	writer.write("psiStrength", val.psiStrength);
	writer.write("psiSkill", val.psiSkill);
	writer.write("melee", val.melee);
	writer.write("mana", val.mana);
}

} // namespace OpenXcom
