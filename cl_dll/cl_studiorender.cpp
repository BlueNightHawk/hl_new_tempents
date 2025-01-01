// BlueNightHawk - 2025
// Reference : Xash3D FWGS

#include "cl_dll.h"
#include "PlatformHeaders.h"
#include "SDL2/SDL_opengl.h"

#include "studio.h"
#include "com_model.h"
#include "r_studioint.h"

#include "triangleapi.h"
#include <vector>

#include "cl_tempents.h"

#include "StudioModelRenderer.h"
#include "GameStudioModelRenderer.h"

extern CGameStudioModelRenderer g_StudioRenderer;
extern engine_studio_api_s IEngineStudio;

/*
================
StudioSetupModel
================
*/
void StudioSetupModel(int bodypart, void** _ppbodypart, void** _ppsubmodel)
{
	auto e = g_StudioRenderer.m_pCurrentEntity;
	studiohdr_t* pStudioHeader = (studiohdr_t*)IEngineStudio.Mod_Extradata(g_StudioRenderer.m_pRenderModel);

	int index;

	mstudiobodyparts_t** ppBodyPart;
	mstudiomodel_t** ppSubModel;

	IEngineStudio.StudioSetupModel(bodypart, (void**)&ppBodyPart, (void**)&ppSubModel);

	if (bodypart > pStudioHeader->numbodyparts)
		bodypart = 0;

	auto pBodyPart = (mstudiobodyparts_t*)((byte*)pStudioHeader + pStudioHeader->bodypartindex) + bodypart;

	index = e->curstate.body / pBodyPart->base;
	index = index % pBodyPart->nummodels;

	auto pSubModel = (mstudiomodel_t*)((byte*)pStudioHeader + pBodyPart->modelindex) + index;

	*ppBodyPart = pBodyPart;
	*ppSubModel = pSubModel;

	*_ppbodypart = pBodyPart;
	*_ppsubmodel = pSubModel;
}

/*
================
CL_StudioDrawTempEnt
================
*/
void CL_StudioDrawTempEnt(cl_entity_s* pEntity)
{
	g_StudioRenderer.m_pCurrentEntity = pEntity;

	pEntity->curstate.origin = pEntity->origin;
	pEntity->curstate.angles = pEntity->angles;

	if (pEntity->curstate.rendermode != kRenderNormal)
		IEngineStudio.StudioSetRenderamt(pEntity->curstate.renderamt);
	else
		IEngineStudio.StudioSetRenderamt(255);

	g_StudioRenderer.StudioDrawModel(STUDIO_RENDER | STUDIO_EVENTS);
}

/*
================
DrawStudioTempEnts
================
*/
int CTempEntManager::DrawStudioTempEnts(bool bTrans)
{
	auto eng_curent = IEngineStudio.GetCurrentEntity();
	int renderamt = eng_curent->curstate.renderamt;

	for (auto f : bTrans ? m_TransVisEnts : m_VisEnts)
	{
		if (f->model->type == mod_studio)
		{
			CL_StudioDrawTempEnt(f);
			f->prevstate = f->curstate;
		}
	}

	if (eng_curent->curstate.rendermode != kRenderNormal)
		IEngineStudio.StudioSetRenderamt(renderamt);
	else
		IEngineStudio.StudioSetRenderamt(255);

	return 1;
}

/*
================
StudioDrawModel
================
*/
int CTempEntManager::StudioDrawModel(int flags)
{
	if (IEngineStudio.GetCurrentEntity() == &m_DummyEnt)
	{
		return DrawStudioTempEnts(false);
	}
	else if (IEngineStudio.GetCurrentEntity() == &m_DummyTransEnt)
	{
		return DrawStudioTempEnts(true);
	}

	return 0;
}
