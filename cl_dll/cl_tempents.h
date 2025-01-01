#pragma once

// BlueNightHawk - 2025
// Reference : Xash3D FWGS

#include <vector>
#include "cvardef.h"

#define MAX_TEMPENTS 2048

class CTempEntManager
{
public:
	// HUD_Init
	void Init();

	// HUD_VidInit
	void InitTempEnts();

	// HUD_TempEntUpdate
	void SetupTempEnts(TEMPENTITY** ppTempEntFree, TEMPENTITY** ppTempEntActive);
	// HUD_TempEntUpdate -> Replace Callback_AddVisibleEntity
	static int AddVisibleEntity(cl_entity_t* ent);
	// HUD_TempEntUpdate -> Call at the very bottom of the function
	void SortTransparentEntities();

	// R_StudioDrawModel
	// NOTE : Don't forget to look at the changes
	// in CStudioModelRenderer::StudioDrawModel and
	// R_StudioDrawModel
	int StudioDrawModel(int flags);
	
	// HUD_DrawTransparentTriangles
	void DrawSpriteTempEnts();

public:
	// Fill this in from V_CalcRefDef function
	ref_params_s refdef;

private:
	TEMPENTITY m_TempEnts[MAX_TEMPENTS];
	cvar_t *cl_tempents;

	std::vector<cl_entity_t*> m_VisEnts;
	std::vector<cl_entity_t*> m_TransVisEnts;

	cl_entity_t m_DummyEnt;
	cl_entity_t m_DummyTransEnt;

	bool m_bInitDone = false;

	int DrawStudioTempEnts(bool bTrans);
};

// StudioRenderFinal_Hardware -> Replace IEngineStudio.StudioSetupModel
void StudioSetupModel(int bodypart, void** _ppbodypart, void** _ppsubmodel);

inline CTempEntManager g_TempEntMan;