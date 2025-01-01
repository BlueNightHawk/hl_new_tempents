// BlueNightHawk - 2025
// Reference : Xash3D FWGS

#include "cl_dll.h"
#include "cl_util.h"

#include "studio.h"
#include "com_model.h"
#include "r_studioint.h"

#include "triangleapi.h"
#include <vector>

#include "cl_tempents.h"

#define R_ModelOpaque(rm) (rm == kRenderNormal)

#define BIT(n) (1U << (n))
#define MODEL_TRANSPARENT BIT(3) // have transparent surfaces
#define VectorAverage(a, b, o) ((o)[0] = ((a)[0] + (b)[0]) * 0.5f, (o)[1] = ((a)[1] + (b)[1]) * 0.5f, (o)[2] = ((a)[2] + (b)[2]) * 0.5f)

extern engine_studio_api_s IEngineStudio;

static int R_RankForRenderMode(int rendermode)
{
	switch (rendermode)
	{
	case kRenderTransTexture:
		return 1; // draw second
	case kRenderTransAdd:
		return 2; // draw third
	case kRenderGlow:
		return 3; // must be last!
	}
	return 0;
}

/*
================
R_GetEntityRenderMode

check for texture flags
================
*/
int R_GetEntityRenderMode(cl_entity_t* ent)
{
	int i, opaque, trans;
	mstudiotexture_t* ptexture;
	model_t* model = NULL;
	studiohdr_t* phdr;

	if (ent->player) // check it for real playermodel
		model = IEngineStudio.SetupPlayerModel(ent->curstate.number - 1);

	if (!model)
		model = ent->model;

	if (!model)
		return ent->curstate.rendermode;

	if (model->type == mod_sprite)
		return ent->curstate.rendermode;

	if ((phdr = (studiohdr_t*)IEngineStudio.Mod_Extradata(model)) == NULL)
	{
		if (R_ModelOpaque(ent->curstate.rendermode))
		{
			// forcing to choose right sorting type
			if ((model && model->type == mod_brush) && (model->flags & MODEL_TRANSPARENT))
				return kRenderTransAlpha;
		}
		return ent->curstate.rendermode;
	}
	ptexture = (mstudiotexture_t*)((byte*)phdr + phdr->textureindex);

	for (opaque = trans = i = 0; i < phdr->numtextures; i++, ptexture++)
	{
		// ignore chrome & additive it's just a specular-like effect
		if ((ptexture->flags & STUDIO_NF_ADDITIVE) && !(ptexture->flags & STUDIO_NF_CHROME))
			trans++;
		else
			opaque++;
	}

	// if model is more additive than opaque
	if (trans > opaque)
		return kRenderTransAdd;
	return ent->curstate.rendermode;
}

/*
===============
R_OpaqueEntity

Opaque entity can be brush or studio model but sprite
===============
*/
qboolean R_OpaqueEntity(cl_entity_t* ent)
{
	if (R_GetEntityRenderMode(ent) == kRenderNormal)
	{
		switch (ent->curstate.renderfx)
		{
		case kRenderFxNone:
		case kRenderFxDeadPlayer:
		case kRenderFxLightMultiplier:
		case kRenderFxExplode:
			return true;
		}
	}
	return false;
}

/*
===============
R_TransEntityCompare

Sorting translucent entities by rendermode then by distance
===============
*/
int R_TransEntityCompare(const void* a, const void* b)
{
	cl_entity_t *ent1, *ent2;
	Vector vecLen, org;
	float dist1, dist2;
	int rendermode1;
	int rendermode2;

	ent1 = *(cl_entity_t**)a;
	ent2 = *(cl_entity_t**)b;
	rendermode1 = R_GetEntityRenderMode(ent1);
	rendermode2 = R_GetEntityRenderMode(ent2);

	// sort by distance
	if (ent1->model->type != mod_brush || rendermode1 != kRenderTransAlpha)
	{
		VectorAverage(ent1->model->mins, ent1->model->maxs, org);
		VectorAdd(ent1->origin, org, org);
		VectorSubtract(g_TempEntMan.refdef.vieworg, org, vecLen);
		dist1 = DotProduct(vecLen, vecLen);
	}
	else
		dist1 = 1000000000;

	if (ent2->model->type != mod_brush || rendermode2 != kRenderTransAlpha)
	{
		VectorAverage(ent2->model->mins, ent2->model->maxs, org);
		VectorAdd(ent2->origin, org, org);
		VectorSubtract(g_TempEntMan.refdef.vieworg, org, vecLen);
		dist2 = DotProduct(vecLen, vecLen);
	}
	else
		dist2 = 1000000000;

	if (dist1 > dist2)
		return -1;
	if (dist1 < dist2)
		return 1;

	// then sort by rendermode
	if (R_RankForRenderMode(rendermode1) > R_RankForRenderMode(rendermode2))
		return 1;
	if (R_RankForRenderMode(rendermode1) < R_RankForRenderMode(rendermode2))
		return -1;

	return 0;
}

/*
=================
SortTransparentEntities
=================
*/
void CTempEntManager::SortTransparentEntities()
{
	std::qsort(m_TransVisEnts.data(), m_TransVisEnts.size(), sizeof(cl_entity_t*), R_TransEntityCompare);
}

/*
=================
CL_AddVisibleEntity

Add to client side renderer queue, don't take up engine's queue
=================
*/
int CTempEntManager::AddVisibleEntity(cl_entity_t* ent)
{
	Vector mins = ent->origin + ent->model->mins;
	Vector maxs = ent->origin + ent->model->maxs;

	if (!gEngfuncs.pTriAPI->BoxInPVS(mins, maxs))
		return 0;

	if ((int)g_TempEntMan.cl_tempents->value <= 0)
	{
		return gEngfuncs.CL_CreateVisibleEntity(2, ent);
	}

	if (R_OpaqueEntity(ent))
	{
		g_TempEntMan.m_VisEnts.push_back(ent);
	}
	else
	{
		g_TempEntMan.m_TransVisEnts.push_back(ent);
	}

	return 1;
}