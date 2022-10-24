CaveBase_MapScripts::
	map_script MAP_SCRIPT_ON_TRANSITION, CaveBase_MapScripts_MAP_SCRIPT_ON_TRANSITION
	.byte 0

CaveBase_MapScripts_MAP_SCRIPT_ON_TRANSITION:
	special PlaceItemBall
	clearflag FLAG_MAZE_ITEM_BALL_1
	return


Common_RandomItemBall::
	random 100
	goto_if_lt VAR_RESULT, 5, Common_DoItemBallEncounter
	special ChooseRandomItem
	callstd STD_FIND_ITEM
	end


Common_DoItemBallEncounter::
	setwildbattle SPECIES_VOLTORB, 5
	playmoncry SPECIES_VOLTORB, CRY_MODE_ENCOUNTER
	delay 20
	waitmoncry
	dowildbattle
	fadescreenswapbuffers FADE_TO_BLACK
	removeobject VAR_LAST_TALKED
	fadescreenswapbuffers FADE_FROM_BLACK
	end

