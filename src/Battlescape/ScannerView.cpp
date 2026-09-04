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
#include "ScannerView.h"
#include "../Engine/Game.h"
#include "../Engine/SurfaceSet.h"
#include "../Mod/Mod.h"
#include "../Engine/Action.h"
#include "../Savegame/BattleUnit.h"
#include "../Savegame/Tile.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/SavedBattleGame.h"
#include "../Battlescape/BattlescapeGame.h"
#include "../fmath.h"

namespace OpenXcom
{

/**
 * Initializes the Scanner view.
 * @param w The ScannerView width.
 * @param h The ScannerView height.
 * @param x The ScannerView x origin.
 * @param y The ScannerView y origin.
 * @param game Pointer to the core game.
 * @param unit The current unit.
 */
ScannerView::ScannerView (int w, int h, int x, int y, Game * game, BattleUnit *unit) : InteractiveSurface(w, h, x, y), _game(game), _unit(unit), _frame(0)
{
	_redraw = true;
}

/**
 * Draws the ScannerView view.
 */
void ScannerView::draw()
{
	SurfaceSet *set = _game->getMod()->getSurfaceSet("DETBLOB.DAT");
	Surface *surface = 0;

	
	const BattleItem* scanner = _game->getSavedGame()->getSavedBattle()->getBattleGame()->getCurrentAction()->weapon; //_unit->getUtilityWeapon(BT_SCANNER); //_unit->getActiveHand(_unit->getLeftHandWeapon(), _unit->getRightHandWeapon());
	const int scanRad    = scanner ? scanner->getRules()->getScanRange() : 9;
	const bool isScanAll = scanner ? scanner->getRules()->isScanAll() : false;

	clear();
	this->lock();

	for (int x = -scanRad; x <= scanRad; x++)
	{
		for (int y = -scanRad; y <= scanRad; y++)
		{
			for (int z = 0; z < _game->getSavedGame()->getSavedBattle()->getMapSizeZ(); z++)
			{
				Tile *t = _game->getSavedGame()->getSavedBattle()->getTile(Position(x,y,z) + Position(_unit->getPosition().x, _unit->getPosition().y, 0));
				if (t && t->getUnit() && (t->getUnit()->getMotionPoints() || isScanAll))
				{
					int frame = (t->getUnit()->getMotionPoints() / 5);
					if (frame >= 0)
					{
						t->getUnit()->setScannedTurn(_game->getSavedGame()->getSavedBattle()->getTurn());
						if (frame > 5) frame = 5;
						surface = set->getFrame(frame + _frame);
						int cX = Clamp<Sint8>(x, -9, 9);
						int cY = Clamp<Sint8>(y, -9, 9);
						//surface->blitNShade(this, ((9+cX)*8)-4, ((9+cY)*8)-4, (_action->actor->getPosition().z == t->getPosition().z || _frame == 0) ? 0 : 4);
						surface->blitNShade(this, ((9+cX)*8)-4, ((9+cY)*8)-4, (_unit->getPosition().z == t->getPosition().z) ? 0 : _frame * 4);
					}
				}
			}
		}
	}

	// the arrow of the direction the unit is pointed
	surface = set->getFrame(7 + _unit->getDirection());

	surface->blitNShade(this, (9*8)-4, (9*8)-4, 0);
	this->unlock();


}

/**
 * Handles clicks on the scanner view.
 * @param action Pointer to an action.
 * @param state State that the action handlers belong to.
 */
void ScannerView::mouseClick (Action *, State *)
{
}

/**
 * Updates the scanner animation.
 */
void ScannerView::animate()
{
	_frame++;
	if (_frame > 1)
	{
		_frame = 0;
	}
	_redraw = true;
}

}
