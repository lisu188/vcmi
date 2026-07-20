/*
 * NewTurnProcessor.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#pragma once

#include "../../lib/constants/EntityIdentifiers.h"
#include "../../lib/constants/Enumerations.h"
#include "../../lib/gameState/RumorState.h"

class CGTownInstance;
class ResourceSet;
struct SetAvailableCreatures;
struct SetMovePoints;
struct SetMana;
struct InfoWindow;
struct NewTurn;

class CGameHandler;

class NewTurnProcessor : boost::noncopyable
{
	CGameHandler * gameHandler;

	std::vector<SetMana> updateHeroesManaPoints();
	std::vector<SetMovePoints> updateHeroesMovementPoints();

	ResourceSet generatePlayerIncome(PlayerColor playerID, bool newWeek);
	SetAvailableCreatures generateTownGrowth(const CGTownInstance * town, EWeekType weekType, CreatureID creatureWeek, bool firstDay, int additionalGrowth);
	RumorState pickNewRumor();
	InfoWindow createInfoWindow(EWeekType weekType, CreatureID creatureWeek, bool newMonth, int additionalGrowth);
	std::tuple<EWeekType, CreatureID, int> pickWeekType(bool newMonth);

	NewTurn generateNewTurnPack();
	void handleTimeEvents(PlayerColor player);
	void handleTownEvents(const CGTownInstance *town);

	void updateNeutralTownGarrison(const CGTownInstance * t, int currentWeek) const;

	/// Fires all scripts subscribed to ScriptEventKind::ON_PLAYER_TURN_START for the given player.
	/// Server-side, data-only (identifiers passed as JSON); script exceptions are caught and logged.
	void firePlayerTurnStartHooks(PlayerColor which);

public:
	NewTurnProcessor(CGameHandler * gameHandler);

	void onNewTurn();
	void onPlayerTurnStarted(PlayerColor color);
	void onPlayerTurnEnded(PlayerColor color);
};
