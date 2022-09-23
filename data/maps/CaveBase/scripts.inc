CaveBase_MapScripts::
	map_script MAP_SCRIPT_ON_TRANSITION, CaveBase_MapScripts_MAP_SCRIPT_ON_TRANSITION
	.byte 0

CaveBase_MapScripts_MAP_SCRIPT_ON_TRANSITION:
	setobjectxyperm 1, 2, 2
	return


TestScript::
	special ChooseRandomItem
	giveitem VAR_RESULT
	removeobject 1
	release
	end

